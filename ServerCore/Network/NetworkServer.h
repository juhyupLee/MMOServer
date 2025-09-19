#pragma once

class NetworkSession;
struct NetworkTask;

class NetworkServer : public Singleton<NetworkServer>
{
public:

public:
	NetworkServer();
	~NetworkServer();
	bool Initialize();
	void Convert(char* buffer, int32_t bufferSize);

public:
	void Listen(int32_t port, JobDispatcher* jobDispatcher);
	void Connect(std::string ip, int32_t port, JobDispatcher* jobDispatcher);
	bool RegisterSocketToIOCP(SOCKET socket);
	bool InitializeWorkerThread(DWORD workerThreadCount);
	std::shared_ptr<NetworkSession> CreateNewSession(JobDispatcher* jobDispatcher);
	bool RemoveSession(int64_t sessionID);
	std::shared_ptr<NetworkSession> FindSession(int64_t sessionID);


private:
	static void WorkerThread();
	
public:
	static void Crash();
	bool WorkerPush(NetworkTask* networkTask);
	HANDLE GetIOCP();
private:
	LockFreeStack<uint64_t>* m_IndexStack;

private:
	std::recursive_mutex m_lock;
	std::unordered_map<int64_t, std::shared_ptr<NetworkSession>> m_sessions;
	std::vector<std::thread> m_workerThread;
	DWORD m_WorkerThreadCount;
	HANDLE m_IOCP;
};

