
#include "NetworkServer.h"

NetworkServer::NetworkServer()
	: m_WorkerThreadCount(0)
{
	setlocale(LC_ALL, "");
}

NetworkServer::~NetworkServer()
{
	if (m_workGuard)
	{
		m_workGuard.reset();
	}
	m_ioContext.stop();
	for (auto& t : m_workerThread)
	{
		if (t.joinable())
		{
			t.join();
		}
	}
}

bool NetworkServer::Initialize()
{
	m_workGuard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
		boost::asio::make_work_guard(m_ioContext));

	InitializeWorkerThread(5);
	return true;
}

void NetworkServer::Convert(char* buffer, int32_t bufferSize)
{
	const std::string g_key = "MMORPG - MOBIRIX";

	uint8_t* pos = (uint8_t*)buffer;
	uint8_t key = 0;

	for (int32_t n = 0; n < bufferSize; ++n)
	{
		key = (key + static_cast<uint8_t>(g_key.at(n % g_key.size()))) * 253 + 195;
		*pos = (*pos) ^ key;
		++pos;
	}
}

bool NetworkServer::InitializeWorkerThread(uint32_t workerThreadCount)
{
	m_WorkerThreadCount = workerThreadCount;

	for (size_t i = 0; i < m_WorkerThreadCount; ++i)
	{
		m_workerThread.emplace_back([this]()
		{
			m_ioContext.run();
		});
	}
	return true;
}

std::shared_ptr<NetworkSession> NetworkServer::CreateNewSession(JobDispatcher* jobDispatcher)
{
	std::lock_guard guard(m_lock);
	auto newSession = MakeMySharedPtr<NetworkSession>(m_ioContext, jobDispatcher);
	auto it = m_sessions.insert({ newSession->GetSessionID(), newSession });
	if (it.second == false)
	{
		return nullptr;
	}
	return newSession;
}

bool NetworkServer::RemoveSession(int64_t sessionID)
{
	std::lock_guard guard(m_lock);
	auto findSession = FindSession(sessionID);
	if (findSession == nullptr)
	{
		return false;
	}
	findSession->Disconnect();
	m_sessions.erase(sessionID);
	return true;
}

std::shared_ptr<NetworkSession> NetworkServer::FindSession(int64_t sessionID)
{
	std::lock_guard guard(m_lock);
	auto findIt = m_sessions.find(sessionID);
	return findIt == m_sessions.end() ? nullptr : findIt->second;
}

void NetworkServer::Listen(int32_t port, JobDispatcher* jobDispatcher)
{
	try
	{
		auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), static_cast<unsigned short>(port));
		auto acceptor = std::make_shared<boost::asio::ip::tcp::acceptor>(m_ioContext, endpoint);

		acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
		acceptor->set_option(boost::asio::ip::tcp::no_delay(true));

		m_acceptors.push_back(acceptor);

		LOG_INFO("Listen on port:%", port);
		DoAccept(acceptor, jobDispatcher);
	}
	catch (const boost::system::system_error& e)
	{
		LOG_ERR("Listen failed:%", e.what());
	}
}

void NetworkServer::DoAccept(std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor, JobDispatcher* jobDispatcher)
{
	acceptor->async_accept(
		[this, acceptor, jobDispatcher](boost::system::error_code ec, boost::asio::ip::tcp::socket socket)
		{
			if (!ec)
			{
				auto remoteEndpoint = socket.remote_endpoint();
				auto ip = remoteEndpoint.address().to_string();
				auto port = static_cast<int32_t>(remoteEndpoint.port());

				std::cout << "IP: " << ip << std::endl;
				LOG_INFO("IP:%", ip);

				auto session = CreateNewSession(jobDispatcher);
				if (session != nullptr)
				{
					session->OnAccept(std::move(socket), ip, port);
				}
			}
			else
			{
				LOG_ERR("Accept failed:%", ec.message());
			}

			DoAccept(acceptor, jobDispatcher);
		}
	);
}

void NetworkServer::Connect(std::string ip, int32_t port, JobDispatcher* jobDispatcher)
{
	auto session = CreateNewSession(jobDispatcher);
	if (session == nullptr)
	{
		LOG_ERR("CreateSession failed");
		return;
	}

	session->Connect(ip, port);
}

boost::asio::io_context& NetworkServer::GetIOContext()
{
	return m_ioContext;
}
