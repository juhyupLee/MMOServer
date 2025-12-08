#pragma once

class NetworkSession;
struct NetworkTask;

class NetworkServer : public Singleton<NetworkServer>
{
public:
	NetworkServer();
	~NetworkServer();
	bool Initialize();
	bool InitializeWorkerThread(DWORD workerThreadCount);

public:
	//Network IO
	void Listen(int32_t port, JobDispatcher* jobDispatcher);
	void Connect(std::string ip, int32_t port, JobDispatcher* jobDispatcher);
	bool RegisterSocketToIOCP(SOCKET socket);

public:
	//Session
	std::shared_ptr<NetworkSession> CreateNewSession(JobDispatcher* jobDispatcher);
	bool RemoveSession(int64_t sessionID);
	std::shared_ptr<NetworkSession> FindSession(int64_t sessionID);

	//Encrypt Decrypt
	void Convert(char* buffer, int32_t bufferSize);

public:
	bool WorkerPush(NetworkTask* networkTask);

public:
	// Getter Setter
	HANDLE GetIOCP();

private:
	static void WorkerThread();
private:
	std::unordered_map<int64_t, std::shared_ptr<NetworkSession>> m_sessions;
	std::recursive_mutex m_lock;
	std::vector<std::thread> m_workerThread;
	DWORD m_WorkerThreadCount;
	HANDLE m_IOCP;
};

