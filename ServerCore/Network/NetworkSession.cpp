#include "NetworkSession.h"

#include "FrameDecoder.h"
#include "MessageJob.h"
#include "../Utill/LogManager.h"
#include "../Utill/UIDGenerator.h"

#include <array>
#include <utility>

#if defined(__linux__)
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

NetworkSession::NetworkSession(
	boost::asio::io_context& ioContext,
	std::shared_ptr<JobDispatcherHandle> jobDispatcherHandle,
	std::size_t maxPendingSendBytes,
	std::size_t maxPendingJobCount)
	: m_sessionID(UIDGenerator::GetInstance()->GenerateSessionID())
	, m_jobQueue(std::make_shared<JobQueue>(
		std::move(jobDispatcherHandle),
		maxPendingJobCount))
	, m_strand(boost::asio::make_strand(ioContext))
	, m_socket(ioContext)
	, m_resolver(ioContext)
	, m_frameDeadline(ioContext)
	, m_maxPendingSendBytes(maxPendingSendBytes)
{
	s_liveObjectCount.fetch_add(1, std::memory_order_relaxed);
	m_frameBuffer.reserve(FrameDecoder::MAX_FRAME_SIZE);
}

NetworkSession::~NetworkSession()
{
	s_liveObjectCount.fetch_sub(1, std::memory_order_relaxed);
}

bool NetworkSession::Disconnect()
{
	return NetworkServer::GetInstance()->RemoveSession(m_sessionID);
}

void NetworkSession::Connect(std::string ip, std::int32_t port)
{
	auto self = shared_from_this();
	boost::asio::post(
		m_strand,
		[self, ip = std::move(ip), port]() mutable
		{
			if (self->m_closeStarted)
			{
				return;
			}

			self->SetRemoteEndpoint(ip, port);
			self->m_resolver.async_resolve(
				ip,
				std::to_string(port),
				boost::asio::bind_executor(
					self->m_strand,
					[self](boost::system::error_code error, Tcp::resolver::results_type endpoints)
					{
						if (self->m_closeStarted)
						{
							return;
						}

						if (error)
						{
							self->Fail("resolve", error);
							return;
						}

						boost::asio::async_connect(
							self->m_socket,
							endpoints,
							boost::asio::bind_executor(
								self->m_strand,
								[self](boost::system::error_code connectError, const Tcp::endpoint& endpoint)
								{
									if (self->m_closeStarted)
									{
										return;
									}

									if (connectError)
									{
										self->Fail("connect", connectError);
										return;
									}

									self->SetRemoteEndpoint(
										endpoint.address().to_string(),
										static_cast<std::int32_t>(endpoint.port()));
									self->OnConnected();
								}));
					}));
		});
}

void NetworkSession::OnAccept(Tcp::socket socket, std::string ip, std::int32_t port)
{
	auto self = shared_from_this();
	boost::asio::post(
		m_strand,
		[self, socket = std::move(socket), ip = std::move(ip), port]() mutable
		{
			if (self->m_closeStarted)
			{
				boost::system::error_code ignored;
				socket.close(ignored);
				return;
			}

			self->m_socket = std::move(socket);
			self->SetRemoteEndpoint(std::move(ip), port);
			self->ConfigureSocket();
			{
				std::lock_guard sendGuard(self->m_sendAdmissionLock);
				self->m_acceptingSends = true;
			}
			self->m_isConnected.store(true, std::memory_order_release);

			FConnectAckT connectedMessage;
			connectedMessage.result = EResultID::R_SUCCESS;
			self->Send(connectedMessage);
			self->StartReceive();
		});
}

void NetworkSession::OnConnected()
{
	if (m_closeStarted)
	{
		return;
	}

	ConfigureSocket();
	{
		std::lock_guard sendGuard(m_sendAdmissionLock);
		m_acceptingSends = true;
	}
	m_isConnected.store(true, std::memory_order_release);
	StartReceive();
}

void NetworkSession::ConfigureSocket()
{
	boost::system::error_code error;
	m_socket.set_option(Tcp::no_delay(true), error);
	if (error)
	{
		LOG_WARN("TCP_NODELAY failed:%", error.message());
	}

	error.clear();
	m_socket.set_option(boost::asio::socket_base::keep_alive(true), error);
	if (error)
	{
		LOG_WARN("SO_KEEPALIVE failed:%", error.message());
	}

#if defined(__linux__)
	// Detect black-holed peers in about one minute instead of inheriting the
	// host's multi-hour TCP keepalive defaults. Application heartbeat can still
	// impose a tighter game-specific deadline later.
	const int keepIdleSeconds = 30;
	const int keepIntervalSeconds = 10;
	const int keepProbeCount = 3;
	const auto nativeSocket = m_socket.native_handle();
	if (::setsockopt(nativeSocket, IPPROTO_TCP, TCP_KEEPIDLE,
		&keepIdleSeconds, sizeof(keepIdleSeconds)) != 0
		|| ::setsockopt(nativeSocket, IPPROTO_TCP, TCP_KEEPINTVL,
			&keepIntervalSeconds, sizeof(keepIntervalSeconds)) != 0
		|| ::setsockopt(nativeSocket, IPPROTO_TCP, TCP_KEEPCNT,
			&keepProbeCount, sizeof(keepProbeCount)) != 0)
	{
		LOG_WARN("TCP keepalive tuning failed. session=%", m_sessionID);
	}
#endif
}

void NetworkSession::StartReceive()
{
	if (m_closeStarted || !m_socket.is_open())
	{
		return;
	}

	const auto segments = m_recvRingQueue.GetWritableSegments();
	const auto writableBytes = segments[0].size + segments[1].size;
	if (writableBytes == 0)
	{
		Fail("receive buffer full", boost::asio::error::no_buffer_space);
		return;
	}

	const std::array<boost::asio::mutable_buffer, 2> buffers{
		boost::asio::buffer(segments[0].data, segments[0].size),
		boost::asio::buffer(segments[1].data, segments[1].size),
	};

	auto self = shared_from_this();
	m_socket.async_read_some(
		buffers,
		boost::asio::bind_executor(
			m_strand,
			[self](boost::system::error_code error, std::size_t bytesTransferred)
			{
				if (self->m_closeStarted)
				{
					return;
				}

				if (error || bytesTransferred == 0)
				{
					if (!error)
					{
						error = boost::asio::error::eof;
					}
					self->Fail("receive", error);
					return;
				}

				if (!self->ProcessReceivedData(bytesTransferred))
				{
					return;
				}

				self->StartReceive();
			}));
}

bool NetworkSession::ProcessReceivedData(std::size_t bytesTransferred)
{
	if (!m_recvRingQueue.CommitWrite(bytesTransferred))
	{
		Fail("receive buffer commit", boost::asio::error::message_size);
		return false;
	}

	while (true)
	{
		std::uint32_t messageSize = 0;
		switch (FrameDecoder::TryDecode(m_recvRingQueue, m_frameBuffer, messageSize))
		{
		case FrameDecodeResult::NeedMoreData:
			if (m_recvRingQueue.GetReadSize() > 0)
			{
				ArmFrameDeadline();
			}
			else
			{
				CancelFrameDeadline();
			}
			return true;

		case FrameDecodeResult::ProtocolError:
			NetworkServer::GetInstance()->IncProtocolError();
			Fail("invalid frame", boost::asio::error::message_size);
			return false;

		case FrameDecodeResult::FrameReady:
			CancelFrameDeadline();
			NetworkServer::GetInstance()->Convert(
				m_frameBuffer.data() + PACKET_HEADER_SIZE,
				static_cast<std::int32_t>(messageSize));

			if (!OnRecvMessage(m_frameBuffer.data(), static_cast<std::int32_t>(messageSize)))
			{
				return false;
			}
			break;
		}
	}
}

void NetworkSession::ArmFrameDeadline()
{
	if (m_frameDeadlineArmed || m_closeStarted)
	{
		return;
	}

	m_frameDeadlineArmed = true;
	m_frameDeadline.expires_after(std::chrono::seconds(10));
	auto self = shared_from_this();
	m_frameDeadline.async_wait(boost::asio::bind_executor(
		m_strand,
		[self](const boost::system::error_code& error)
		{
			if (error == boost::asio::error::operation_aborted
				|| self->m_closeStarted || !self->m_frameDeadlineArmed)
			{
				return;
			}
			self->m_frameDeadlineArmed = false;
			NetworkServer::GetInstance()->IncProtocolError();
			self->Fail("frame assembly timeout", boost::asio::error::timed_out);
		}));
}

void NetworkSession::CancelFrameDeadline()
{
	if (!m_frameDeadlineArmed)
	{
		return;
	}
	m_frameDeadlineArmed = false;
	try
	{
		m_frameDeadline.cancel();
	}
	catch (const boost::system::system_error&)
	{
		// CloseInternal will cancel the socket as a second teardown barrier.
	}
}

bool NetworkSession::OnRecvMessage(char* messageBuffer, std::int32_t messageSize)
{
	flatbuffers::Verifier verifier(
		reinterpret_cast<std::uint8_t*>(messageBuffer),
		PACKET_HEADER_SIZE + static_cast<std::size_t>(messageSize));
	if (!VerifySizePrefixedMessageHolderBuffer(verifier))
	{
		NetworkServer::GetInstance()->IncProtocolError();
		Fail("invalid flatbuffer", boost::asio::error::invalid_argument);
		return false;
	}

	const auto* packedMessageHolder = GetSizePrefixedMessageHolder(messageBuffer);
	const auto messageType = packedMessageHolder->message_type();
	if (messageType == MessageID::NONE
		|| EnumNameMessageID(messageType)[0] == '\0'
		|| packedMessageHolder->message() == nullptr)
	{
		NetworkServer::GetInstance()->IncProtocolError();
		Fail("invalid message type", boost::asio::error::invalid_argument);
		return false;
	}

	auto messageHolder = PacketHolder(packedMessageHolder->UnPack());
	if (!m_jobQueue->Push(std::make_shared<MessageJob>(messageHolder, m_sessionID)))
	{
		LOG_WARN("Receive job queue limit or dispatcher lifetime exceeded. session=% pending=%",
			m_sessionID,
			static_cast<std::int64_t>(m_jobQueue->GetPendingJobCount()));
		Fail("receive job queue", boost::asio::error::no_buffer_space);
		return false;
	}
	NetworkServer::GetInstance()->IncRecvPacket(
		PACKET_HEADER_SIZE + static_cast<std::size_t>(messageSize));
	return true;
}

bool NetworkSession::EnqueueSend(std::shared_ptr<NetworkPacket> packet)
{
	auto self = shared_from_this();
	const auto packetSize = packet->Size();
	if (packetSize > FrameDecoder::MAX_FRAME_SIZE)
	{
		LOG_WARN("Outbound frame exceeds protocol limit. session=% packet=% limit=%",
			m_sessionID,
			static_cast<std::int64_t>(packetSize),
			static_cast<std::int64_t>(FrameDecoder::MAX_FRAME_SIZE));
		return false;
	}
	std::size_t pendingBytes = 0;
	bool closeForBackpressure = false;
	bool closeForPostFailure = false;

	{
		// Reservation and post are one admission operation. CloseInternal takes
		// this same lock before disabling sends, so no packet can be posted after
		// the close operation has drained.
		std::lock_guard admissionGuard(m_sendAdmissionLock);
		if (!m_acceptingSends)
		{
			return false;
		}

		pendingBytes = m_pendingSendBytes.load(std::memory_order_relaxed);
		if (packetSize > m_maxPendingSendBytes
			|| pendingBytes > m_maxPendingSendBytes - packetSize)
		{
			m_acceptingSends = false;
			closeForBackpressure = true;
		}
		else
		{
			m_pendingSendBytes.fetch_add(packetSize, std::memory_order_relaxed);
			try
			{
				boost::asio::post(
					m_strand,
					[self, packet = std::move(packet)]() mutable
					{
						if (self->m_closeStarted)
						{
							return;
						}
						if (!self->m_socket.is_open())
						{
							self->Fail("send on closed socket", boost::asio::error::not_connected);
							return;
						}

						const bool startWrite = self->m_sendQueue.empty();
						self->m_sendQueue.emplace_back(std::move(packet));
						if (startWrite)
						{
							self->StartSend();
						}
					});
			}
			catch (const std::exception& exception)
			{
				m_pendingSendBytes.fetch_sub(packetSize, std::memory_order_relaxed);
				m_acceptingSends = false;
				closeForPostFailure = true;
				LOG_ERR("Send post failed. session=% error=%", m_sessionID, exception.what());
			}
		}
	}

	if (closeForBackpressure)
	{
		NetworkServer::GetInstance()->IncBackpressureDisconnect();
		LOG_WARN("Send queue limit exceeded. session=% pending=% packet=% limit=%",
			m_sessionID,
			static_cast<std::int64_t>(pendingBytes),
			static_cast<std::int64_t>(packetSize),
			static_cast<std::int64_t>(m_maxPendingSendBytes));
	}

	if (closeForBackpressure || closeForPostFailure)
	{
		Close();
		NetworkServer::GetInstance()->RemoveSession(m_sessionID);
	}

	return !closeForBackpressure && !closeForPostFailure;
}

void NetworkSession::StartSend()
{
	if (m_closeStarted || !m_socket.is_open() || m_sendQueue.empty())
	{
		return;
	}

	auto packet = m_sendQueue.front();
	auto self = shared_from_this();
	boost::asio::async_write(
		m_socket,
		boost::asio::buffer(packet->m_buffer.data(), packet->m_buffer.size()),
		boost::asio::bind_executor(
			m_strand,
			[self, packet](boost::system::error_code error, std::size_t)
			{
				if (self->m_closeStarted)
				{
					return;
				}

				if (error)
				{
					self->Fail("send", error);
					return;
				}

				if (self->m_sendQueue.empty() || self->m_sendQueue.front() != packet)
				{
					self->Fail("send queue state", boost::asio::error::operation_not_supported);
					return;
				}

				const auto packetSize = packet->Size();
				self->m_sendQueue.pop_front();
				self->m_pendingSendBytes.fetch_sub(packetSize, std::memory_order_relaxed);
				NetworkServer::GetInstance()->IncSendPacket(packetSize);

				if (!self->m_sendQueue.empty())
				{
					self->StartSend();
				}
			}));
}

void NetworkSession::Close()
{
	auto self = shared_from_this();
	boost::asio::dispatch(m_strand, [self]()
	{
		self->CloseInternal();
	});
}

void NetworkSession::CloseInternal()
{
	if (m_closeStarted)
	{
		return;
	}

	m_closeStarted = true;
	m_isConnected.store(false, std::memory_order_release);
	{
		std::lock_guard admissionGuard(m_sendAdmissionLock);
		m_acceptingSends = false;
		m_pendingSendBytes.store(0, std::memory_order_relaxed);
	}
	m_resolver.cancel();
	CancelFrameDeadline();

	boost::system::error_code ignored;
	m_socket.cancel(ignored);
	m_socket.shutdown(Tcp::socket::shutdown_both, ignored);
	m_socket.close(ignored);

	m_sendQueue.clear();
	m_recvRingQueue.ClearBuffer();
}

void NetworkSession::Fail(const char* operation, const boost::system::error_code& error)
{
	if (m_closeStarted && error == boost::asio::error::operation_aborted)
	{
		return;
	}

	LOG_WARN("Network operation failed. operation=% session=% error=%",
		operation,
		m_sessionID,
		error.message());
	NetworkServer::GetInstance()->IncNetworkError();

	CloseInternal();
	NetworkServer::GetInstance()->RemoveSession(m_sessionID);
}

void NetworkSession::SetRemoteEndpoint(std::string ip, std::int32_t port)
{
	std::lock_guard guard(m_endpointLock);
	m_ip = std::move(ip);
	m_port = port;
}

std::string NetworkSession::GetIP() const
{
	std::lock_guard guard(m_endpointLock);
	return m_ip;
}

std::int32_t NetworkSession::GetPort() const
{
	std::lock_guard guard(m_endpointLock);
	return m_port;
}

JobDispatcher* NetworkSession::GetJobDispatcher()
{
	return m_jobQueue->GetJobDispatcher();
}
