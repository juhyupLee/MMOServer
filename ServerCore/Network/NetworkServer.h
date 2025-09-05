#pragma once

//------------------------------------------------------
// 0 : Non Log
// 1 : g_Log
// 2 : Session Log && g_Log 
#define MEMORYLOG_USE 0
//------------------------------------------------------
//------------------------------------------------------
// 0 : Non Log
// 1 : 로그 사용
#define DISCONLOG_USE 0
//------------------------------------------------------

#define SERVER_NAME L"MMOGameServer"

enum eSessionStatus
{
	NOT_USED,
	AUTH,
	AUTH_TO_GAME,
	GAME,
	AUTH_RELEASE,
	GAME_RELEASE,
	RELEASE
};


class NetworkSession;
struct NetworkTask;

class NetworkServer : public Singleton<NetworkServer>
{
public:

	struct ConnectInfo
	{
		SOCKET _Socket;
		WCHAR _IP[17];
		uint16_t _Port;
	};

public:
	NetworkServer();
	~NetworkServer();
	bool Initialize();

public:
	void Listen(int32_t port, JobDispatcher* jobDispatcher);
	bool RegisterSocketToIOCP(SOCKET socket);
	//bool Start(WCHAR* ip, uint16_t port, DWORD runningThread, SocketOption& option, DWORD workerThreadCount, DWORD maxUserCount, TimeOutOption& timeOutOption, NetworkSession** sessionArray);
	//void ServerStop();
	//bool NetworkInit(WCHAR* ip, uint16_t port, DWORD runningThread, SocketOption option);
	//bool EventInit();
	bool InitializeWorkerThread(DWORD workerThreadCount);
	std::shared_ptr<NetworkSession> CreateNewSession(JobDispatcher* jobDispatcher);

	//bool DisconnectAllUser();

	//static void SpecialErrorCodeCheck(int32_t errorCode);


	//uint64_t GetSessionID(uint64_t index);
	//uint16_t GetSessionIndex(uint64_t sessionID);

	//-----------------------------------------------------------------------------
	// Release 관련 정리함수들
	//-----------------------------------------------------------------------------
	//void ReleaseSession(NetworkSession* delSession);
	//void ReleaseSocket(NetworkSession* session);
	//void ReleasePacket(NetworkSession* session);
	//void DeQPacket(NetworkSession* session);
	//void SessionClear(NetworkSession* session);
	

	//-----------------------------------------------------------------------------
	// Timeout, 보내고 끊기 함수들
	//-----------------------------------------------------------------------------
	//void SetTimeOut(NetworkSession* session);
	//void SetTimeOut(NetworkSession* session, DWORD timeOut);
	//void SendNDiscon(NetworkSession* session, NetPacket* packet);

public:

	//virtual bool OnConnectionRequest(WCHAR* ip, uint16_t port) = 0;
	//void AcceptUser(SOCKET socket, WCHAR* ip, uint16_t port);



	//bool RecvPacket(NetworkSession* curSession, DWORD transferByte);
	//bool RecvPost(NetworkSession* curSession);

	//-----------------------------------------------------------------------------
	// 모니터링 Getter
	//-----------------------------------------------------------------------------
	int64_t GetAcceptCount();
	LONG GetAcceptTPS();
	LONG GetSendTPS();
	LONG GetRecvTPS();
	LONG GetNetworkTraffic();
	LONG GetNetworkRecvTraffic();
	LONG GetSessionCount();
	//int32_t GetMemoryAllocCount();
	LONG GetSendQMeomryCount();
	LONG GetLockFreeStackMemoryCount();
	DWORD GetAuthFPS();
	DWORD GetGameFPS();
	DWORD GetSendFPS();

protected:

	//void SetSessionArray(NetworkSession* sessionArray,DWORD maxUserCount);

private:
	//-----------------------------------------------------------------------
	// MMOG GameLib에서 관리하는 스레드 
	//-----------------------------------------------------------------------
	//static unsigned int __stdcall AcceptThread(LPVOID param);
	static void WorkerThread(LPVOID param);
	//static unsigned int __stdcall MonitorThread(LPVOID param);
	//static unsigned int __stdcall AuthThread(LPVOID param);
	//static unsigned int __stdcall SendThread(LPVOID param);

	
public:
	static void Crash();
	bool WorkerPush(NetworkTask* networkTask);
	HANDLE GetIOCP();
private:
	LockFreeStack<uint64_t>* m_IndexStack;
	

	TimeOutOption m_TimeOutOption;
	ObjectPool<ConnectInfo> m_ConInfoPool;
	TemplateQ<ConnectInfo*> m_SocketQ;

	uint64_t m_SessionID;
protected:
	DWORD m_MaxUserCount;
	LONG m_SessionCount;
	NetworkSession** m_SessionArray;

	LONG m_AuthSessionCount;
	LONG m_GameSessionCount;


private:

	SOCKET m_ListenSocket;
	HANDLE* m_WorkerThread;
	std::vector<std::thread> m_workerThread;
	HANDLE m_AcceptThread;
	HANDLE m_MonitoringThread;
	HANDLE m_GameThread;
	HANDLE m_AuthThread;
	HANDLE m_SendThread;

	uint16_t m_ServerPort;
	WCHAR* m_ServerIP;


	DWORD m_WorkerThreadCount;

	HANDLE m_IOCP;

	//------------------------------------------------
	// For Debugging
	//------------------------------------------------
	DWORD m_AuthFPS_To_Main;
	DWORD m_GameFPS_To_Main;
	DWORD m_SendFPS_To_Main;

	LONG m_NetworkTraffic;
	LONG m_NetworkRecvTraffic;

	//MyMemoryLog<int64_t> m_MemoryLog_Overlap;

	int64_t m_SendFlagNo;

	LONG m_RecvTPS;
	LONG m_SendTPS;
	LONG m_AcceptTPS;


	LONG m_RecvTPS_To_Main;
	LONG m_SendTPS_To_Main;
	LONG m_AcceptTPS_To_Main;
	LONG m_NetworkTraffic_To_Main;
	LONG m_NetworkRecvTraffic_To_Main;

	LONG m_SendQMemory;


	int64_t m_AcceptCount;

	NetworkServer* m_Contents;

	HANDLE m_MonitorEvent;
	HANDLE m_GameThreadEvent;
	HANDLE m_AuthTreadEvent;
	HANDLE m_SendThreadEvent;

};

