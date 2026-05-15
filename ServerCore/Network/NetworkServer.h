#pragma once
#include <boost/asio.hpp>

class NetworkSession;

class NetworkServer : public Singleton<NetworkServer>
{
public:
	NetworkServer();
	~NetworkServer();
	bool Initialize();
	bool InitializeWorkerThread(uint32_t workerThreadCount);

public:
	//Network IO
	void Listen(int32_t port, JobDispatcher* jobDispatcher);
	void Connect(std::string ip, int32_t port, JobDispatcher* jobDispatcher);

public:
	//Session
	std::shared_ptr<NetworkSession> CreateNewSession(JobDispatcher* jobDispatcher);
	bool RemoveSession(int64_t sessionID);
	std::shared_ptr<NetworkSession> FindSession(int64_t sessionID);

	//Encrypt Decrypt
	void Convert(char* buffer, int32_t bufferSize);

public:
	boost::asio::io_context& GetIOContext();

private:
	void DoAccept(std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor, JobDispatcher* jobDispatcher);

private:
	std::unordered_map<int64_t, std::shared_ptr<NetworkSession>> m_sessions;
	std::recursive_mutex m_lock;
	std::vector<std::thread> m_workerThread;
	uint32_t m_WorkerThreadCount;

	boost::asio::io_context m_ioContext;
	std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_workGuard;
	std::vector<std::shared_ptr<boost::asio::ip::tcp::acceptor>> m_acceptors;
};
