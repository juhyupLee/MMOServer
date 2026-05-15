#include "MessageJob.h"

NetworkSession::NetworkSession(boost::asio::io_context& ioContext, JobDispatcher* jobDispatcher)
	: m_socket(ioContext)
	, m_sessionID(UIDGenerator::GetInstance()->GenerateSessionID())
	, m_port(0)
	, m_isSending(false)
{
	m_jobQueue = MakeMySharedPtr<JobQueue>(jobDispatcher);
}

bool NetworkSession::Disconnect()
{
	boost::system::error_code ec;
	if (m_socket.is_open())
	{
		m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		m_socket.close(ec);
	}
	return true;
}

int64_t NetworkSession::GetSessionID()
{
	return m_sessionID;
}

void NetworkSession::Connect(std::string ip, int32_t port)
{
	m_ip = ip;
	m_port = port;

	auto resolver = std::make_shared<boost::asio::ip::tcp::resolver>(m_socket.get_executor());
	resolver->async_resolve(ip, std::to_string(port),
		[this, self = shared_from_this(), resolver](boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results)
		{
			if (ec)
			{
				LOG_ERR("Resolve failed:%", ec.message());
				return;
			}

			boost::asio::async_connect(m_socket, results,
				[this, self](boost::system::error_code ec, const boost::asio::ip::tcp::endpoint&)
				{
					if (ec)
					{
						LOG_ERR("Connect failed:%", ec.message());
						return;
					}

					// Set socket options
					boost::system::error_code optEc;
					m_socket.set_option(boost::asio::ip::tcp::no_delay(true), optEc);

					OnConnected();
				}
			);
		}
	);
}

void NetworkSession::OnConnected()
{
	DoReceive();
}

std::string NetworkSession::GetIP()
{
	return m_ip;
}

int32_t NetworkSession::GetPort()
{
	return m_port;
}

JobDispatcher* NetworkSession::GetJobDispatcher()
{
	return m_jobQueue->GetJobDispatcher();
}

LockFreeQ<NetworkPacket*>& NetworkSession::GetSendQueue()
{
	return m_sendQueue;
}

RingQ& NetworkSession::GetRecvRingQueue()
{
	return m_recvRingQueue;
}

void NetworkSession::OnAccept(boost::asio::ip::tcp::socket socket, std::string ip, int32_t port)
{
	m_socket = std::move(socket);
	m_ip = ip;
	m_port = port;

	// Set socket options
	boost::system::error_code ec;
	m_socket.set_option(boost::asio::ip::tcp::no_delay(true), ec);

	m_socket.set_option(boost::asio::socket_base::linger(true, 0), ec);

	GSCL_CONNECTED_CMDT msg;
	Send(msg);

	DoReceive();
}

void NetworkSession::OnRecvMessage(char* messageBuffer, int32_t messageSize)
{
	flatbuffers::Verifier verifier((uint8_t*)messageBuffer, PACKET_HEADER_SIZE + messageSize);
	if (VerifySizePrefixedMessageHolderBuffer(verifier) == false)
	{
		return;
	}
	const auto packedMessageHolder = GetSizePrefixedMessageHolder(messageBuffer);
	const auto messageID = packedMessageHolder->message_type();

	const auto messageHolder = PacketHolder(packedMessageHolder->UnPack());
	m_jobQueue->Push(MakeMySharedPtr<MessageJob>(messageHolder, m_sessionID));
}

void NetworkSession::DoReceive()
{
	if (m_recvRingQueue.GetWriteSize() <= 0)
	{
		return;
	}

	// Get writable buffer segments from ring buffer
	std::vector<BufferSegment> segments;
	m_recvRingQueue.GetDirectEnQData(segments);

	if (segments.empty())
	{
		return;
	}

	// Convert to boost::asio buffer sequence
	std::vector<boost::asio::mutable_buffer> buffers;
	buffers.reserve(segments.size());
	for (auto& seg : segments)
	{
		buffers.emplace_back(boost::asio::buffer(seg.buf, seg.len));
	}

	m_socket.async_read_some(buffers,
		[this, self = shared_from_this()](boost::system::error_code ec, size_t bytesTransferred)
		{
			if (ec || bytesTransferred <= 0)
			{
				if (ec != boost::asio::error::operation_aborted)
				{
					LOG_ERR("Recv failed:%", ec.message());
					NetworkServer::GetInstance()->RemoveSession(m_sessionID);
				}
				return;
			}

			ProcessReceivedData(bytesTransferred);
		}
	);
}

void NetworkSession::ProcessReceivedData(size_t bytesTransferred)
{
	m_recvRingQueue.MoveWitePosition(static_cast<int>(bytesTransferred));

	while (true)
	{
		int packetSize = m_recvRingQueue.GetReadSize();
		if (packetSize >= PACKET_HEADER_SIZE)
		{
			char* packet = m_recvRingQueue.GetReadBufferPtr();
			int32_t messageSize = flatbuffers::ReadScalar<flatbuffers::uoffset_t>(packet);

			if (0 <= messageSize && messageSize <= packetSize - PACKET_HEADER_SIZE)
			{
				m_recvRingQueue.MoveReadPosition(PACKET_HEADER_SIZE + messageSize);
				OnRecvMessage(packet, messageSize);
				continue;
			}
		}

		break;
	}

	DoReceive();
}

void NetworkSession::DoSend()
{
	if (m_isSending.exchange(true) == true)
	{
		return;
	}

	// Dequeue all pending packets
	{
		std::lock_guard<std::mutex> guard(m_sendMutex);
		for (auto* p : m_pendingSendPackets)
		{
			delete p;
		}
		m_pendingSendPackets.clear();

		NetworkPacket* networkPacket = nullptr;
		while (m_sendQueue.DeQ(&networkPacket) == true)
		{
			m_pendingSendPackets.push_back(networkPacket);
		}
	}

	if (m_pendingSendPackets.empty())
	{
		m_isSending.store(false);
		return;
	}

	// Build buffer sequence
	std::vector<boost::asio::const_buffer> buffers;
	buffers.reserve(m_pendingSendPackets.size());
	for (auto* pkt : m_pendingSendPackets)
	{
		buffers.emplace_back(boost::asio::buffer(pkt->m_buffer.data(), pkt->m_buffer.size()));
	}

	boost::asio::async_write(m_socket, buffers,
		[this, self = shared_from_this()](boost::system::error_code ec, size_t bytesTransferred)
		{
			if (ec)
			{
				LOG_ERR("Send failed:%", ec.message());
				m_isSending.store(false);
				return;
			}

			// Clean up sent packets
			{
				std::lock_guard<std::mutex> guard(m_sendMutex);
				for (auto* p : m_pendingSendPackets)
				{
					delete p;
				}
				m_pendingSendPackets.clear();
			}

			m_isSending.store(false);

			// Check if more data queued while we were sending
			if (m_sendQueue.GetQCount() > 0)
			{
				DoSend();
			}
		}
	);
}
