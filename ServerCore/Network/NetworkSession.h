#pragma once

#include "JobQueue.h"
#include "NetworkServer.h"
#include "../Memory/Global.h"
#include "../Memory/RingBuffer.h"

#include <boost/asio.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct NetworkPacket
{
	flatbuffers::DetachedBuffer m_buffer{};

	std::size_t Size() const noexcept { return m_buffer.size(); }
};

class NetworkSession : public std::enable_shared_from_this<NetworkSession>
{
	friend class NetworkServer;

public:
	NetworkSession(
		boost::asio::io_context& ioContext,
		std::shared_ptr<JobDispatcherHandle> jobDispatcherHandle,
		std::size_t maxPendingSendBytes,
		std::size_t maxPendingJobCount);
	~NetworkSession();

	bool Disconnect();

	template <MessageConcept T>
	bool Send(T& message);

	std::int64_t GetSessionID() const noexcept { return m_sessionID; }
	bool IsConnected() const noexcept { return m_isConnected.load(std::memory_order_acquire); }

	void Connect(std::string ip, std::int32_t port);

	std::string GetIP() const;
	std::int32_t GetPort() const;
	JobDispatcher* GetJobDispatcher();
	std::size_t GetPendingSendBytes() const noexcept
	{
		return m_pendingSendBytes.load(std::memory_order_relaxed);
	}
	static std::int64_t GetLiveObjectCount() noexcept
	{
		return s_liveObjectCount.load(std::memory_order_relaxed);
	}

	bool OnRecvMessage(char* messageBuffer, std::int32_t messageSize);

private:
	using Tcp = boost::asio::ip::tcp;
	using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

	void OnAccept(Tcp::socket socket, std::string ip, std::int32_t port);
	void OnConnected();
	void ConfigureSocket();

	void StartReceive();
	bool ProcessReceivedData(std::size_t bytesTransferred);
	void ArmFrameDeadline();
	void CancelFrameDeadline();

	bool EnqueueSend(std::shared_ptr<NetworkPacket> packet);
	void StartSend();

	void Close();
	void CloseInternal();
	void Fail(const char* operation, const boost::system::error_code& error);
	void SetRemoteEndpoint(std::string ip, std::int32_t port);

private:
	const std::int64_t m_sessionID;
	std::shared_ptr<JobQueue> m_jobQueue;

	Strand m_strand;
	Tcp::socket m_socket;
	Tcp::resolver m_resolver;
	boost::asio::steady_timer m_frameDeadline;

	RingQ m_recvRingQueue;
	std::vector<char> m_frameBuffer;
	std::deque<std::shared_ptr<NetworkPacket>> m_sendQueue;
	std::atomic<std::size_t> m_pendingSendBytes{ 0 };
	const std::size_t m_maxPendingSendBytes;
	std::mutex m_sendAdmissionLock;
	bool m_acceptingSends{ false }; // Protected by m_sendAdmissionLock.

	mutable std::mutex m_endpointLock;
	std::string m_ip;
	std::int32_t m_port{ 0 };

	std::atomic<bool> m_isConnected{ false };
	bool m_closeStarted{ false }; // Accessed only through m_strand.
	bool m_frameDeadlineArmed{ false }; // Accessed only through m_strand.

	inline static std::atomic<std::int64_t> s_liveObjectCount{ 0 };
};

template <MessageConcept T>
bool NetworkSession::Send(T& message)
{
	if (!IsConnected())
	{
		return false;
	}

	auto packet = std::make_shared<NetworkPacket>();
	auto messageHolder = CreateMessageHolder(message);

	flatbuffers::FlatBufferBuilder builder;
	auto offset = MessageHolder::Pack(builder, messageHolder.get());
	builder.FinishSizePrefixed(offset);
	packet->m_buffer = builder.Release();

	const auto messageSize = static_cast<std::int32_t>(packet->m_buffer.size() - PACKET_HEADER_SIZE);
	if (messageSize > 0)
	{
		NetworkServer::GetInstance()->Convert(
			reinterpret_cast<char*>(packet->m_buffer.data()) + PACKET_HEADER_SIZE,
			messageSize);
	}

	return EnqueueSend(std::move(packet));
}
