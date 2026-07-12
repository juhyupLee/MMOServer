#pragma once

#include "../Utill/Singleton.h"

#include <boost/asio.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class JobDispatcher;
struct JobDispatcherHandle;
class NetworkSession;

class NetworkServer : public Singleton<NetworkServer>
{
public:
	NetworkServer();
	~NetworkServer();

	bool Initialize(
		std::uint32_t workerThreadCount = 0,
		std::size_t maxPendingSendBytesPerSession = 256 * 1024,
		std::size_t maxPendingJobsPerSession = 256,
		std::size_t maxSessionCount = 4096);
	void Shutdown();

	std::uint16_t Listen(std::int32_t port, JobDispatcher* jobDispatcher);
	std::shared_ptr<NetworkSession> Connect(
		std::string ip,
		std::int32_t port,
		JobDispatcher* jobDispatcher);

	std::shared_ptr<NetworkSession> CreateNewSession(JobDispatcher* jobDispatcher);
	bool RemoveSession(std::int64_t sessionID);
	std::shared_ptr<NetworkSession> FindSession(std::int64_t sessionID);

	void Convert(char* buffer, std::int32_t bufferSize);

	void IncRecvPacket(std::size_t bytes = 0) noexcept
	{
		m_recvPacketCount.fetch_add(1, std::memory_order_relaxed);
		m_recvByteCount.fetch_add(bytes, std::memory_order_relaxed);
	}
	void IncSendPacket(std::size_t bytes = 0) noexcept
	{
		m_sendPacketCount.fetch_add(1, std::memory_order_relaxed);
		m_sendByteCount.fetch_add(bytes, std::memory_order_relaxed);
	}
	void IncProtocolError() noexcept
	{
		m_protocolErrorCount.fetch_add(1, std::memory_order_relaxed);
	}
	void IncNetworkError() noexcept
	{
		m_networkErrorCount.fetch_add(1, std::memory_order_relaxed);
	}
	void IncBackpressureDisconnect() noexcept
	{
		m_backpressureDisconnectCount.fetch_add(1, std::memory_order_relaxed);
	}
	std::int64_t GetRecvCount() const noexcept { return m_recvPacketCount.load(std::memory_order_relaxed); }
	std::int64_t GetSendCount() const noexcept { return m_sendPacketCount.load(std::memory_order_relaxed); }
	std::int64_t GetRecvByteCount() const noexcept { return m_recvByteCount.load(std::memory_order_relaxed); }
	std::int64_t GetSendByteCount() const noexcept { return m_sendByteCount.load(std::memory_order_relaxed); }
	std::int64_t GetCreatedSessionCount() const noexcept { return m_createdSessionCount.load(std::memory_order_relaxed); }
	std::int64_t GetRemovedSessionCount() const noexcept { return m_removedSessionCount.load(std::memory_order_relaxed); }
	std::int64_t GetProtocolErrorCount() const noexcept { return m_protocolErrorCount.load(std::memory_order_relaxed); }
	std::int64_t GetNetworkErrorCount() const noexcept { return m_networkErrorCount.load(std::memory_order_relaxed); }
	std::int64_t GetBackpressureDisconnectCount() const noexcept
	{
		return m_backpressureDisconnectCount.load(std::memory_order_relaxed);
	}
	std::int64_t GetLiveSessionObjectCount() const noexcept;
	std::size_t GetSessionCount() const;
	std::uint32_t GetWorkerThreadCount() const noexcept
	{
		return m_workerThreadCount.load(std::memory_order_relaxed);
	}

private:
	using Tcp = boost::asio::ip::tcp;
	using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

	void DoAccept(
		const std::shared_ptr<Tcp::acceptor>& acceptor,
		const std::shared_ptr<JobDispatcherHandle>& jobDispatcherHandle);
	std::shared_ptr<NetworkSession> CreateNewSession(
		const std::shared_ptr<JobDispatcherHandle>& jobDispatcherHandle);
	std::uint32_t ResolveWorkerThreadCount(std::uint32_t requestedCount) const noexcept;

private:
	mutable std::mutex m_sessionLock;
	std::unordered_map<std::int64_t, std::shared_ptr<NetworkSession>> m_sessions;
	std::mutex m_acceptorLock;
	std::shared_mutex m_lifecycleLock;

	boost::asio::io_context m_ioContext;
	std::unique_ptr<WorkGuard> m_workGuard;
	std::vector<std::jthread> m_workerThreads;
	std::vector<std::shared_ptr<Tcp::acceptor>> m_acceptors;

	std::atomic<bool> m_initialized{ false };
	std::atomic<std::uint32_t> m_workerThreadCount{ 0 };
	std::size_t m_maxPendingSendBytesPerSession{ 256 * 1024 };
	std::size_t m_maxPendingJobsPerSession{ 256 };
	std::size_t m_maxSessionCount{ 4096 };

	std::atomic<std::int64_t> m_recvPacketCount{ 0 };
	std::atomic<std::int64_t> m_sendPacketCount{ 0 };
	std::atomic<std::int64_t> m_recvByteCount{ 0 };
	std::atomic<std::int64_t> m_sendByteCount{ 0 };
	std::atomic<std::int64_t> m_createdSessionCount{ 0 };
	std::atomic<std::int64_t> m_removedSessionCount{ 0 };
	std::atomic<std::int64_t> m_protocolErrorCount{ 0 };
	std::atomic<std::int64_t> m_networkErrorCount{ 0 };
	std::atomic<std::int64_t> m_backpressureDisconnectCount{ 0 };
};
