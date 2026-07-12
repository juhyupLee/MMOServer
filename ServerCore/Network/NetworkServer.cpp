#include "NetworkServer.h"

#include "JobDispatcher.h"
#include "NetworkSession.h"
#include "../Utill/LogManager.h"

#include <algorithm>
#include <chrono>
#include <locale>
#include <utility>

namespace
{
thread_local bool g_isNetworkWorkerThread = false;

bool IsDispatcherAlive(const std::shared_ptr<JobDispatcherHandle>& handle)
{
	if (handle == nullptr)
	{
		return false;
	}

	std::lock_guard guard(handle->lock);
	return handle->dispatcher != nullptr;
}
}

NetworkServer::NetworkServer()
{
	setlocale(LC_ALL, "");
}

NetworkServer::~NetworkServer()
{
	Shutdown();
}

bool NetworkServer::Initialize(
	std::uint32_t workerThreadCount,
	std::size_t maxPendingSendBytesPerSession,
	std::size_t maxPendingJobsPerSession,
	std::size_t maxSessionCount)
{
	std::lock_guard lifecycleGuard(m_lifecycleLock);
	if (m_initialized.load(std::memory_order_acquire))
	{
		return true;
	}

	const auto resolvedWorkerThreadCount = ResolveWorkerThreadCount(workerThreadCount);
	m_workerThreadCount.store(resolvedWorkerThreadCount, std::memory_order_relaxed);
	m_maxPendingSendBytesPerSession = std::max<std::size_t>(maxPendingSendBytesPerSession, 1);
	m_maxPendingJobsPerSession = std::max<std::size_t>(maxPendingJobsPerSession, 1);
	m_maxSessionCount = std::max<std::size_t>(maxSessionCount, 1);
	m_ioContext.restart();
	m_workGuard = std::make_unique<WorkGuard>(boost::asio::make_work_guard(m_ioContext));

	try
	{
		m_workerThreads.reserve(resolvedWorkerThreadCount);
		for (std::uint32_t i = 0; i < resolvedWorkerThreadCount; ++i)
		{
			m_workerThreads.emplace_back([this]()
			{
				g_isNetworkWorkerThread = true;
				try
				{
					m_ioContext.run();
				}
				catch (const std::exception& exception)
				{
					LOG_ERR("Asio worker exception:%", exception.what());
				}
				g_isNetworkWorkerThread = false;
			});
		}
	}
	catch (const std::exception& exception)
	{
		m_workGuard.reset();
		m_ioContext.stop();
		m_workerThreads.clear();
		m_workerThreadCount.store(0, std::memory_order_relaxed);
		LOG_ERR("Network initialization failed:%", exception.what());
		return false;
	}

	// Publish all configuration and io_context state only after every worker
	// and the work guard have been created successfully.
	m_initialized.store(true, std::memory_order_release);
	LOG_INFO("Network initialized. ioThreads=% maxPendingSendBytes=% maxPendingJobs=% maxSessions=%",
		static_cast<std::int64_t>(resolvedWorkerThreadCount),
		static_cast<std::int64_t>(m_maxPendingSendBytesPerSession),
		static_cast<std::int64_t>(m_maxPendingJobsPerSession),
		static_cast<std::int64_t>(m_maxSessionCount));
	return true;
}

void NetworkServer::Shutdown()
{
	if (g_isNetworkWorkerThread)
	{
		LOG_ERR("NetworkServer::Shutdown must run on a control thread, not an Asio worker");
		return;
	}

	std::lock_guard lifecycleGuard(m_lifecycleLock);
	if (!m_initialized.exchange(false))
	{
		return;
	}

	std::vector<std::shared_ptr<Tcp::acceptor>> acceptors;
	{
		std::lock_guard guard(m_acceptorLock);
		acceptors = std::move(m_acceptors);
	}
	boost::asio::post(m_ioContext, [acceptors = std::move(acceptors)]() mutable
	{
		for (const auto& acceptor : acceptors)
		{
			if (acceptor == nullptr)
			{
				continue;
			}

			boost::system::error_code error;
			acceptor->cancel(error);
			acceptor->close(error);
		}
	});

	std::vector<std::shared_ptr<NetworkSession>> sessions;
	{
		std::lock_guard guard(m_sessionLock);
		sessions.reserve(m_sessions.size());
		for (auto& [id, session] : m_sessions)
		{
			sessions.emplace_back(std::move(session));
		}
		m_sessions.clear();
		m_removedSessionCount.fetch_add(
			static_cast<std::int64_t>(sessions.size()),
			std::memory_order_relaxed);
	}

	for (const auto& session : sessions)
	{
		if (session != nullptr)
		{
			session->Close();
		}
	}

	m_workGuard.reset();
	// Do not stop the context here. Let the posted close operations and their
	// operation_aborted completions drain, then run() exits naturally.
	m_workerThreads.clear();
	m_workerThreadCount.store(0, std::memory_order_relaxed);
}

std::uint16_t NetworkServer::Listen(std::int32_t port, JobDispatcher* jobDispatcher)
{
	std::shared_lock lifecycleGuard(m_lifecycleLock);
	if (!m_initialized.load())
	{
		LOG_ERR("Listen requested before NetworkServer::Initialize");
		return 0;
	}
	if (port < 0 || port > 65535)
	{
		LOG_ERR("Invalid listen port:%", port);
		return 0;
	}
	if (jobDispatcher == nullptr)
	{
		LOG_ERR("Listen requires a live JobDispatcher");
		return 0;
	}

	auto jobDispatcherHandle = jobDispatcher->GetHandle();
	if (!IsDispatcherAlive(jobDispatcherHandle))
	{
		LOG_ERR("Listen requires a live JobDispatcher");
		return 0;
	}

	try
	{
		auto acceptor = std::make_shared<Tcp::acceptor>(m_ioContext);
		const Tcp::endpoint endpoint(Tcp::v4(), static_cast<unsigned short>(port));

		acceptor->open(endpoint.protocol());
		acceptor->set_option(Tcp::acceptor::reuse_address(true));
		acceptor->bind(endpoint);
		acceptor->listen(boost::asio::socket_base::max_listen_connections);
		const auto boundPort = acceptor->local_endpoint().port();

		{
			std::lock_guard guard(m_acceptorLock);
			if (!m_initialized.load())
			{
				boost::system::error_code ignored;
				acceptor->close(ignored);
				return 0;
			}
			m_acceptors.emplace_back(acceptor);
		}
		DoAccept(acceptor, jobDispatcherHandle);
		LOG_INFO("Listen on port:%", boundPort);
		return boundPort;
	}
	catch (const boost::system::system_error& exception)
	{
		LOG_ERR("Listen failed:%", exception.what());
		return 0;
	}
}

void NetworkServer::DoAccept(
	const std::shared_ptr<Tcp::acceptor>& acceptor,
	const std::shared_ptr<JobDispatcherHandle>& jobDispatcherHandle)
{
	std::lock_guard guard(m_acceptorLock);
	if (!m_initialized.load() || !acceptor->is_open()
		|| !IsDispatcherAlive(jobDispatcherHandle))
	{
		return;
	}

	acceptor->async_accept(
		[this, acceptor, jobDispatcherHandle](boost::system::error_code error, Tcp::socket socket)
		{
			if (!error)
			{
				boost::system::error_code endpointError;
				const auto endpoint = socket.remote_endpoint(endpointError);
				const auto ip = endpointError ? std::string{} : endpoint.address().to_string();
				const auto port = endpointError ? 0 : static_cast<std::int32_t>(endpoint.port());

				auto session = CreateNewSession(jobDispatcherHandle);
				if (session != nullptr)
				{
					session->OnAccept(std::move(socket), ip, port);
				}
			}
			else if (error != boost::asio::error::operation_aborted)
			{
				LOG_ERR("Accept failed:%", error.message());
				auto retryTimer = std::make_shared<boost::asio::steady_timer>(m_ioContext);
				retryTimer->expires_after(std::chrono::milliseconds(100));
				retryTimer->async_wait(
					[this, acceptor, jobDispatcherHandle, retryTimer](boost::system::error_code timerError)
					{
						if (!timerError)
						{
							DoAccept(acceptor, jobDispatcherHandle);
						}
					});
				return;
			}

			DoAccept(acceptor, jobDispatcherHandle);
		});
}

std::shared_ptr<NetworkSession> NetworkServer::Connect(
	std::string ip,
	std::int32_t port,
	JobDispatcher* jobDispatcher)
{
	std::shared_lock lifecycleGuard(m_lifecycleLock);
	if (!m_initialized.load())
	{
		LOG_ERR("Connect requested before NetworkServer::Initialize");
		return nullptr;
	}
	if (port <= 0 || port > 65535)
	{
		LOG_ERR("Invalid connect port:%", port);
		return nullptr;
	}
	if (jobDispatcher == nullptr)
	{
		LOG_ERR("Connect requires a live JobDispatcher");
		return nullptr;
	}

	auto session = CreateNewSession(jobDispatcher->GetHandle());
	if (session == nullptr)
	{
		LOG_ERR("CreateSession failed");
		return nullptr;
	}

	session->Connect(std::move(ip), port);
	return session;
}

std::shared_ptr<NetworkSession> NetworkServer::CreateNewSession(JobDispatcher* jobDispatcher)
{
	std::shared_lock lifecycleGuard(m_lifecycleLock);
	if (jobDispatcher == nullptr)
	{
		return nullptr;
	}
	return CreateNewSession(jobDispatcher->GetHandle());
}

std::shared_ptr<NetworkSession> NetworkServer::CreateNewSession(
	const std::shared_ptr<JobDispatcherHandle>& jobDispatcherHandle)
{
	if (!m_initialized.load())
	{
		return nullptr;
	}
	if (!IsDispatcherAlive(jobDispatcherHandle))
	{
		return nullptr;
	}

	auto session = std::make_shared<NetworkSession>(
		m_ioContext,
		jobDispatcherHandle,
		m_maxPendingSendBytesPerSession,
		m_maxPendingJobsPerSession);

	std::lock_guard guard(m_sessionLock);
	// Shutdown flips m_initialized before taking this lock. Rechecking here
	// prevents an accept completion from inserting a session after Shutdown's
	// session snapshot has already been taken.
	if (!m_initialized.load())
	{
		return nullptr;
	}
	if (m_sessions.size() >= m_maxSessionCount)
	{
		return nullptr;
	}
	const auto [it, inserted] = m_sessions.emplace(session->GetSessionID(), session);
	if (inserted)
	{
		m_createdSessionCount.fetch_add(1, std::memory_order_relaxed);
	}
	return inserted ? session : nullptr;
}

bool NetworkServer::RemoveSession(std::int64_t sessionID)
{
	std::shared_ptr<NetworkSession> session;
	{
		std::lock_guard guard(m_sessionLock);
		const auto found = m_sessions.find(sessionID);
		if (found == m_sessions.end())
		{
			return false;
		}

		session = std::move(found->second);
		m_sessions.erase(found);
		m_removedSessionCount.fetch_add(1, std::memory_order_relaxed);
	}

	if (session != nullptr)
	{
		session->Close();
	}
	return true;
}

std::int64_t NetworkServer::GetLiveSessionObjectCount() const noexcept
{
	return NetworkSession::GetLiveObjectCount();
}

std::shared_ptr<NetworkSession> NetworkServer::FindSession(std::int64_t sessionID)
{
	std::lock_guard guard(m_sessionLock);
	const auto found = m_sessions.find(sessionID);
	return found == m_sessions.end() ? nullptr : found->second;
}

std::size_t NetworkServer::GetSessionCount() const
{
	std::lock_guard guard(m_sessionLock);
	return m_sessions.size();
}

void NetworkServer::Convert(char* buffer, std::int32_t bufferSize)
{
	static constexpr char keyText[] = "MMORPG - MOBIRIX";
	constexpr auto keySize = sizeof(keyText) - 1;

	auto* position = reinterpret_cast<std::uint8_t*>(buffer);
	std::uint8_t key = 0;
	for (std::int32_t i = 0; i < bufferSize; ++i)
	{
		key = static_cast<std::uint8_t>(
			(key + static_cast<std::uint8_t>(keyText[i % keySize])) * 253 + 195);
		*position ^= key;
		++position;
	}
}

std::uint32_t NetworkServer::ResolveWorkerThreadCount(std::uint32_t requestedCount) const noexcept
{
	if (requestedCount > 0)
	{
		return requestedCount;
	}

	const auto hardwareThreads = std::thread::hardware_concurrency();
	return std::clamp(hardwareThreads == 0 ? 1U : hardwareThreads, 1U, 4U);
}
