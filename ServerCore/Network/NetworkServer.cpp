
#include "NetworkServer.h"

NetworkServer::NetworkServer()
{
	setlocale(LC_ALL, "");
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		//_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"WSAStartUp() Error:%d", WSAGetLastError());
	}

}

NetworkServer::~NetworkServer()
{
}

bool NetworkServer::Initialize()
{
	m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 5);
	if (m_IOCP == nullptr || m_IOCP == INVALID_HANDLE_VALUE)
	{
		m_IOCP = INVALID_HANDLE_VALUE;
		return false;
	}

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

bool NetworkServer::InitializeWorkerThread(DWORD workerThreadCount)
{
	m_WorkerThreadCount = workerThreadCount;

	for (size_t i = 0; i < m_WorkerThreadCount; ++i)
	{
		m_workerThread.emplace_back([this]()
		{
			NetworkServer::WorkerThread();
		});
	}
	return true;
}

std::shared_ptr<NetworkSession> NetworkServer::CreateNewSession(JobDispatcher* jobDispatcher)
{
	std::lock_guard guard(m_lock);
	auto newSession = MakeMySharedPtr<NetworkSession>(jobDispatcher);
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
	if(findSession == nullptr)
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
	auto task = new NetworkTaskListen;
	task->m_jobDispatcher = jobDispatcher;
	task->m_port = port;
	WorkerPush(task);
}

void NetworkServer::Connect(std::string ip, int32_t port, JobDispatcher* jobDispatcher)
{
	auto task = new NetworkTaskConnect;
	task->m_ip = ip;
	task->m_port = port;
	task->m_jobDispatcher = jobDispatcher;
	WorkerPush(task);
}


bool NetworkServer::RegisterSocketToIOCP(SOCKET socket)
{
	if (CreateIoCompletionPort((HANDLE)socket, m_IOCP, 0, 0) == nullptr)
	{
		LOG_ERR("IOCP Register Fail:%", WSAGetLastError());
		return false;
	}
	return true;
}

void NetworkServer::WorkerThread()
{
	while (true)
	{
		DWORD transferByte = 0;
		LPOVERLAPPED overlapped = nullptr;
		ULONG_PTR key = 0;
		BOOL gqcsResult = GetQueuedCompletionStatus(NetworkServer::GetInstance()->GetIOCP(), &transferByte, &key, &overlapped, INFINITE);

		//종료 신호: IOCP handle close 또는 PostQueuedCompletionStatus(NULL)
		if (overlapped == nullptr)
		{
			break;
		}

		//IO 완료 or 실패: task에 위임 (실패 시 task가 cleanup)
		auto networkTask = static_cast<NetworkTask*>(overlapped);
		if(networkTask->Run(gqcsResult == TRUE, transferByte) == false)
		{
			delete networkTask;
		}
	}
}

bool NetworkServer::WorkerPush(NetworkTask* networkTask)
{
	PostQueuedCompletionStatus(m_IOCP, 0, NULL, static_cast<LPOVERLAPPED>(networkTask));
	return true;
}

HANDLE NetworkServer::GetIOCP()
{
	return m_IOCP;
}