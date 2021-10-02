

#include "MMOGameLib.h"
#include "Log.h"
#include <WS2tcpip.h>
#include <process.h>
#include "Protocol.h"
#include <locale>
#include "Global.h"
#include <timeapi.h>
#include <locale>


MMOGameLib::MMOGameLib()
	:m_ConInfoPool(500)
{
	setlocale(LC_ALL, "");

	m_SessionID = 0;

	m_AuthSessionCount = 0;
	m_GameSessionCount = 0;

	m_NetworkRecvTraffic = 0;
	m_NetworkTraffic = 0;

}

MMOGameLib::~MMOGameLib()
{
}

bool MMOGameLib::ServerStart(WCHAR* ip, uint16_t port, DWORD runningThread, SocketOption& option, DWORD workerThreadCount, DWORD maxUserCount, TimeOutOption& timeOutOption,MMOSession** sessionArray)
{
	timeBeginPeriod(1);

	if (!NetworkInit(ip, port, runningThread, option))
	{
		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"NetworkInit Fail");
	}

	//-----------------------------------------------------
	// ������ Index�� ������ ������ ���� ���� 
	// ServerStop��, delete
	//-----------------------------------------------------
	m_TimeOutOption = timeOutOption;
	m_IndexStack = new LockFreeStack<uint64_t>;

	m_SessionArray = new MMOSession*[maxUserCount];
	m_MaxUserCount = maxUserCount;
	for (uint64_t index = 0; index < m_MaxUserCount; ++index)
	{
		m_SessionArray[index] = sessionArray[index];
 		m_IndexStack->Push(index);
	}

	EventInit();

	ThreadInit(workerThreadCount);

	if (0 != listen(m_ListenSocket, SOMAXCONN))
	{
		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"listen() error:%d", WSAGetLastError());
		return false;
	}
	_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"IOCP Echo Server Listen......");
	return true;
}

void MMOGameLib::ServerStop()
{
	timeEndPeriod(1);


	//-------------------------------------------------------------
	// 	   Monitor ������ ������ Ŭ�� ���ҽ� ����
	//-------------------------------------------------------------
	// 

	//-----------------------------------------------------------------------
	// Accept Thread Resource ����
	//-----------------------------------------------------------------------
	closesocket(m_ListenSocket);
	DWORD rtn = WaitForSingleObject(m_AcceptThread, INFINITE);
	CloseHandle(m_AcceptThread);

	//-----------------------------------------------------------------------
	// Monitor Thread Resource ����
	//-----------------------------------------------------------------------
	SetEvent(m_MonitorEvent);
	WaitForSingleObject(m_MonitoringThread, INFINITE);
	CloseHandle(m_MonitoringThread);
	CloseHandle(m_MonitorEvent);

	if (DisconnectAllUser())
	{
		for (size_t i = 0; i < m_WorkerThreadCount; ++i)
		{
			PostQueuedCompletionStatus(m_IOCP, 0, NULL, NULL);
			CloseHandle(m_WorkerThread[i]);
		}
		WaitForMultipleObjects(m_WorkerThreadCount, m_WorkerThread, TRUE, INFINITE);
	}

	//-----------------------------------------------------------------------
	// Auth Thread Resource ����
	//-----------------------------------------------------------------------
	SetEvent(m_AuthTreadEvent);
	WaitForSingleObject(m_AuthThread, INFINITE);
	CloseHandle(m_AuthTreadEvent);
	CloseHandle(m_AuthThread);

	//-----------------------------------------------------------------------
	// Game Thread Resource ����
	//-----------------------------------------------------------------------
	SetEvent(m_GameThreadEvent);
	WaitForSingleObject(m_GameThread, INFINITE);
	CloseHandle(m_GameThreadEvent);
	CloseHandle(m_GameThread);

	//-----------------------------------------------------------------------
	// Send Thread Resource ����
	//-----------------------------------------------------------------------
	SetEvent(m_SendThreadEvent);
	WaitForSingleObject(m_SendThread, INFINITE);
	CloseHandle(m_SendThreadEvent);
	CloseHandle(m_SendThread);


	_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"��������:%d", m_SessionCount);
	_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"���� ����");


	delete m_IndexStack;
	m_IndexStack = nullptr;

	m_AcceptCount = 0;
	m_AcceptTPS = 0;
	m_RecvTPS = 0;
	m_SendTPS = 0;
	m_SendQMemory = 0;
	m_AcceptTPS_To_Main = 0;
	m_SendTPS_To_Main = 0;
	m_RecvTPS_To_Main = 0;
}

bool MMOGameLib::NetworkInit(WCHAR* ip, uint16_t port, DWORD runningThread, SocketOption option)
{
	WSAData wsaData;
	SOCKADDR_IN serverAddr;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"WSAStartUp() Error:%d", WSAGetLastError());
		return false;
	}
	m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, runningThread);

	m_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (m_ListenSocket == INVALID_SOCKET)
	{
		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"socket() error:%d", WSAGetLastError());
		return false;
	}

	if (option._SendBufferZero)
	{
		//----------------------------------------------------------------------------
		// �۽Ź��� Zero -->�񵿱� IO ����
		//----------------------------------------------------------------------------
		int optVal = 0;
		int optLen = sizeof(optVal);

		int rtnOpt = setsockopt(m_ListenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&optVal, optLen);
		if (rtnOpt != 0)
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}
	}
	if (option._Linger)
	{
		linger lingerOpt;
		lingerOpt.l_onoff = 1;
		lingerOpt.l_linger = 0;

		int rtnOpt = setsockopt(m_ListenSocket, SOL_SOCKET, SO_LINGER, (const char*)&lingerOpt, sizeof(lingerOpt));
		if (rtnOpt != 0)
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}

	}
	if (option._TCPNoDelay)
	{
		BOOL tcpNodelayOpt = true;

		int rtnOpt = setsockopt(m_ListenSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&tcpNodelayOpt, sizeof(tcpNodelayOpt));
		if (rtnOpt != 0)
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}
	}

	if (option._KeepAliveOption.onoff)
	{
		DWORD recvByte = 0;

		if (0 != WSAIoctl(m_ListenSocket, SIO_KEEPALIVE_VALS, &option._KeepAliveOption, sizeof(tcp_keepalive), NULL, 0, &recvByte, NULL, NULL))
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}
	}
	ZeroMemory(&serverAddr, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);

	if (ip == nullptr)
	{
		serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	}
	else
	{
		InetPton(AF_INET, ip, &serverAddr.sin_addr.S_un.S_addr);
	}

	if (0 != bind(m_ListenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
	{
		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"bind() error:%d", WSAGetLastError());
		return false;
	}

	return true;
}

bool MMOGameLib::EventInit()
{
	m_MonitorEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_GameThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_AuthTreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_SendThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	return true;
}

bool MMOGameLib::ThreadInit(DWORD workerThreadCount)
{
	m_WorkerThreadCount = workerThreadCount;
	m_WorkerThread = new HANDLE[workerThreadCount];

	for (size_t i = 0; i < m_WorkerThreadCount; ++i)
	{
		m_WorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, MMOGameLib::WorkerThread, this, 0, NULL);
	}

	m_AcceptThread = (HANDLE)_beginthreadex(NULL, 0, MMOGameLib::AcceptThread, this, 0, NULL);
	m_MonitoringThread = (HANDLE)_beginthreadex(NULL, 0, MMOGameLib::MonitorThread, this, 0, NULL);
	m_AuthThread = (HANDLE)_beginthreadex(NULL, 0, MMOGameLib::AuthThread, this, 0, NULL);
	m_GameThread = (HANDLE)_beginthreadex(NULL, 0, MMOGameLib::GameLogicThread, this, 0, NULL);
	m_SendThread = (HANDLE)_beginthreadex(NULL, 0, MMOGameLib::SendThread, this, 0, NULL);

	return true;
}

bool MMOGameLib::DisconnectAllUser()
{
	for (size_t i = 0; i < m_MaxUserCount; ++i)
	{
		if (m_SessionArray[i]->_USED)
		{
			ReleaseSocket(m_SessionArray[i]);
		}
	}
	bool bAllUserRelease = true;
	while (true)
	{
		bAllUserRelease = true;
		for (size_t i = 0; i < m_MaxUserCount; ++i)
		{
			if (m_SessionArray[i]->_USED)
			{
				bAllUserRelease = false;
			}
		}
		if (bAllUserRelease)
		{
			for (size_t i = 0; i < m_MaxUserCount; ++i)
			{
				delete m_SessionArray[i];
			}
			delete[] m_SessionArray;

			return true;
		}
	}

	return bAllUserRelease;
}

void MMOGameLib::ReleaseSocket(MMOSession* session)
{
#if MEMORYLOG_USE ==1 ||   MEMORYLOG_USE ==2
	IOCP_Log log;
#endif

	if (0 == InterlockedExchange(&session->_CloseFlag, 1))
	{
		//------------------------------------------------------
		// socket�� ���������, ���� session���ִ� ���Ϻ��� InvalidSocket���� ġȯ�Ѵ�.
		//------------------------------------------------------

		SOCKET temp = session->_Socket;

		session->_Socket = INVALID_SOCKET;

		closesocket(temp);

	}

}

void MMOGameLib::SpecialErrorCodeCheck(int32_t errorCode)
{
	if (errorCode != WSAECONNRESET && errorCode != WSAECONNABORTED && errorCode != WSAENOTSOCK && errorCode != WSAEINTR)
	{
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Special ErrorCode : %d", errorCode);
		MMOGameLib::Crash();
	}
}


uint64_t MMOGameLib::GetSessionID(uint64_t index)
{
	return (index << 48) | (++m_SessionID);
}
uint16_t MMOGameLib::GetSessionIndex(uint64_t sessionID)
{
	uint16_t tempIndex = (uint16_t)((0xffff000000000000 & sessionID) >> 48);

	if (tempIndex <0 || tempIndex >m_MaxUserCount - 1)
	{
		Crash();
	}
	return tempIndex;
}


void MMOGameLib::ReleaseSession(MMOSession* delSession)
{
	if (delSession == nullptr)
	{
		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"delte Session�� ���̴�");
		return;
	}


#if MEMORYLOG_USE ==1 ||   MEMORYLOG_USE ==2
	IOCP_Log log;
#endif

#if MEMORYLOG_USE ==1 
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::RELEASE_SESSION, GetCurrentThreadId(), delSession->_Socket, delSession->_IOCount, (int64_t)delSession, delSession->_ID, (int64_t)&delSession->_RecvOL, (int64_t)&delSession->_SendOL, delSession->_SendFlag);
	g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::RELEASE_SESSION, GetCurrentThreadId(), delSession->_Socket, delSession->_IOCount, (int64_t)delSession, delSession->_ID, (int64_t)&delSession->_RecvOL, (int64_t)&delSession->_SendOL, delSession->_SendFlag, -1, -1, eRecvMessageType::NOTHING, -1, -1);
	g_MemoryLog_IOCP.MemoryLogging(log);
	delSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif

	//if (delSession->_ErrorCode != ERROR_SEM_TIMEOUT && delSession->_ErrorCode != 64 && delSession->_ErrorCode != 0 && delSession->_ErrorCode != ERROR_OPERATION_ABORTED)
	//{
	//	MMOGameLib::Crash();
	//}
	uint64_t tempSessionID = delSession->_ID;

	if (delSession->_ErrorCode == ERROR_SEM_TIMEOUT)
	{
		InterlockedIncrement(&g_TCPTimeoutReleaseCnt);
	}

	//---------------------------------------
	// CAS�� IOCOUNT�� �ֻ�����Ʈ��  ReleaseFlag�� ����
	//---------------------------------------

	ReleaseSocket(delSession);


	if (delSession->_IOCount > 0)
	{
		MMOGameLib::Crash();
	}

	ReleasePacket(delSession);
	if (delSession->_SendQ.GetQCount() > 0)
	{
		MMOGameLib::Crash();
	}


	if (delSession->_DeQPacketArray[0] != nullptr)
	{
		MMOGameLib::Crash();
	}

	SessionClear(delSession);

	delSession->OnClientLeave_Auth();
	InterlockedDecrement(&m_SessionCount);
}

void MMOGameLib::SessionClear(MMOSession* session)
{
#if MEMORYLOG_USE ==1 ||   MEMORYLOG_USE ==2
	IOCP_Log log;
#endif

	session->_Socket = INVALID_SOCKET;
	int64_t tempID = session->_ID;
	session->_CloseFlag = 0;
	//-------------------------------------------------
	// Release Flag �ʱ�ȭ �� Accept �� WSARecv�� �� IOCount �̸� ������Ŵ
	//-------------------------------------------------
	InterlockedIncrement(&session->_IOCount);

	session->_SendFlag = 0;
	if (session->_SendQ.GetQCount() > 0)
	{
		MMOGameLib::Crash();
	}

	session->_RecvRingQ.ClearBuffer();

	//-----------------------------------------------------------------------
	// SendPacket����,Session�ȿ��ִ� CompleteQ�� �������� ����ȴٸ�
	// �޸𸮰� ���������ְ� �߸��� �޽����� �������ֱ⶧���� �̸� ó���������.
	//-----------------------------------------------------------------------
	NetPacket* deqPacket = nullptr;

	while (session->_CompleteRecvPacketQ.Dequeue(&deqPacket))
	{
		deqPacket->Free(deqPacket);
	}
	session->_CompleteRecvPacketQ.ClearBuffer();

	ZeroMemory(&session->_RecvOL, sizeof(session->_RecvOL));
	ZeroMemory(&session->_SendOL, sizeof(session->_SendOL));


	ZeroMemory(session->_IP, sizeof(session->_IP));
	session->_Port = 0;

	//-----------------------------------------
	// For Debug
	//-----------------------------------------
	//session->_MemoryLog_IOCP.Clear();
	//session->_MemoryLog_SendFlag.Clear();
	session->_SendByte = 0;
	session->_USED = false;
	session->_ErrorCode = 0;
	session->_GQCSRtn = TRUE;
	session->_TransferZero = 0;
	session->_IOFail = false;

	session->_bIOCancel = false;
	session->_bReserveDiscon = false;
	//-----------------------------------------------
	// Session Index ��ȯ
	//-----------------------------------------------
	session->_bReleaseReady = false;
	session->_SessionStatus = eSessionStatus::NOT_USED;
	m_IndexStack->Push(GetSessionIndex(tempID));
}

void MMOGameLib::SetTimeOut(MMOSession* session)
{
	session->_TimeOut = m_TimeOutOption._HeartBeatTimeOut;
}

void MMOGameLib::SetTimeOut(MMOSession* session, DWORD timeOut)
{
	session->_TimeOut = timeOut;
}

void MMOGameLib::SendNDiscon(MMOSession* session, NetPacket* packet)
{
	//---------------------------------------
   // ���� ��, TimeOut �ð��� 2�ʷ� �����Ѵ�
   // Client���� �ִ� ���������� ���ٸ� ������̴�.
   //---------------------------------------
	session->SendUnicast(packet);
	SetTimeOut(session, 2000);
}



void MMOGameLib::DeQPacket(MMOSession* session)
{
	for (int i = 0; i < session->_DeQArraySize; ++i)
	{
		NetPacket* delNetPacket = session->_DeQPacketArray[i];
		session->_DeQPacketArray[i] = nullptr;

		if (0 == delNetPacket->DecrementRefCount())
		{
			delNetPacket->Free(delNetPacket);

			InterlockedIncrement(&g_FreeMemoryCount);

			/*IOCP_Log log;
			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::DEQPACKET_FREE, GetCurrentThreadId(), session->_Socket, session->_IOCount, (int64_t)session, session->_ID,(int64_t)&session->_RecvOL, (int64_t)&session->_SendOL, session->_SendFlag, (int64_t)delNetPacket);
			g_MemoryLog_IOCP.MemoryLogging(log);
			session->_MemoryLog_IOCP.MemoryLogging(log);*/
		}

		//----------------------------------------------
		// Send �Ϸ����� �� ó���Ǵ°��� TPS�� ī���� �Ѵ�.
		//----------------------------------------------
		InterlockedIncrement(&m_SendTPS);
	}
	session->_DeQArraySize = 0;
}

void MMOGameLib::ReleasePacket(MMOSession* session)
{

	NetPacket* delNetPacket = nullptr;

	while (session->_SendQ.DeQ(&delNetPacket))
	{
		if (0 == delNetPacket->DecrementRefCount())
		{
			delNetPacket->Free(delNetPacket);
			InterlockedIncrement(&g_FreeMemoryCount);
			//IOCP_Log log;
			//log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::RELEASEPACKET_FREE, GetCurrentThreadId(), -1, -1, (int64_t)session, -1, -1, -1, session->_SendFlag);
			//g_MemoryLog_IOCP.MemoryLogging(log);

		}
	}

	//-----------------------------------------------------
	// SendPacket ��, SendPost�����ߴµ�
	// ������ ������ ���� ��� Send �Ϸ������� ����������� �������� �� �ִ�
	// �̸� ���� ó���� ����ߵȴ�.
	//-----------------------------------------------------
	if (session->_DeQArraySize > 0)
	{
		DeQPacket(session);
	}
}



void MMOGameLib::AcceptUser(SOCKET socket, WCHAR* ip, uint16_t port)
{
	ConnectInfo* conInfo = m_ConInfoPool.Alloc();

	wcscpy_s(conInfo->_IP, ip);
	conInfo->_Port = port;
	conInfo->_Socket = socket;

	while (!m_SocketQ.Enqueue(conInfo))
	{

	}
}
void MMOGameLib::CreateNewSession(ConnectInfo* conInfo)
{


#if MEMORYLOG_USE ==1 ||   MEMORYLOG_USE ==2
	IOCP_Log log;
#endif
	MMOSession* newSession = nullptr;
	uint64_t index = 0;

	while (!m_IndexStack->Pop(&index))
	{
		wprintf(L"Pop Indexing\n");
	}

	newSession = m_SessionArray[index];

	if (newSession->_SessionStatus != eSessionStatus::NOT_USED)
	{
		Crash();
	}
	if (newSession == nullptr)
	{
		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Session�� ��� ������Դϴ�");
		MMOGameLib::Crash();
	}
	//-----------------------------------------------------------------
	// Accept�� ������ �⺻ IOCount�� 1�̴�
	//-----------------------------------------------------------------

	newSession->_Index = index;
	newSession->_Socket = conInfo->_Socket;


	//-----------------------------------------------------------------
	// SessionID�� ���� ����, �׶� RelaseFlag�� �ʱ�ȭ���ش�.
	//-----------------------------------------------------------------
	newSession->_ID = GetSessionID(index);


#if MEMORYLOG_USE ==1 
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::ACCEPT_NEW_USER, GetCurrentThreadId(), newSession->_Socket, newSession->_IOCount, (int64_t)newSession, newSession->_ID, (int64_t)&newSession->_RecvOL, (int64_t)&newSession->_SendOL, newSession->_SendFlag);
	g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::ACCEPT_NEW_USER, GetCurrentThreadId(), newSession->_Socket, newSession->_IOCount, (int64_t)newSession, newSession->_ID, (int64_t)&newSession->_RecvOL, (int64_t)&newSession->_SendOL, newSession->_SendFlag);
	g_MemoryLog_IOCP.MemoryLogging(log);
	newSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif

	wcscpy_s(newSession->_IP, conInfo->_IP);
	newSession->_Port = conInfo->_Port;
	newSession->_USED = true;
	//--------------------------------------------
	if (m_TimeOutOption._OptionOn)
	{
		newSession->_TimeOut = m_TimeOutOption._LoginTimeOut;
	}
	newSession->_LastRecvTime = timeGetTime();

	//--------------------------------------------

	//------------------------------------------
	// For Debug
	//------------------------------------------
	uint64_t tempOrderIndex = (newSession->_OrderIndex++) % 3;
	newSession->_LastSessionID[tempOrderIndex][0] = newSession->_SessionOrder++;
	newSession->_LastSessionID[tempOrderIndex][1] = newSession->_ID;
	newSession->_LastSessionID[tempOrderIndex][2] = newSession->_Socket;


	m_ConInfoPool.Free(conInfo);

	DWORD tempTransfer;
	DWORD tempFlag = 0;
	BOOL bResult = WSAGetOverlappedResult(newSession->_Socket, &newSession->_RecvOL, &tempTransfer, FALSE, &tempFlag);
	if (!bResult)
	{
		int error = WSAGetLastError();
		if (error == WSA_IO_INCOMPLETE)
		{
			MMOGameLib::Crash();
		}
	}

	if (m_SessionID > UINT64_MAX - 1)
	{
		MMOGameLib::Crash();
	}

	if (NULL == CreateIoCompletionPort((HANDLE)newSession->_Socket, m_IOCP, (ULONG_PTR)newSession, 0))
	{
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"���ϰ� IOCP �������:%d", GetLastError());
		newSession->_USED = false;
		return;
	}

	//_LOG(LOG_LEVEL_DEBUG, L"Sesssion[%d]�� ����Ǿ����ϴ�", newSession->_ID);

	InterlockedIncrement(&m_SessionCount);//m_SessionCount

	newSession->OnClientJoin_Auth(newSession->_IP, newSession->_Port);


	WSABUF tempBuffer[1];
	DWORD flag = 0;

	tempBuffer[0].buf = newSession->_RecvRingQ.GetRearBufferPtr();
	tempBuffer[0].len = newSession->_RecvRingQ.GetDirectEnqueueSize();

	if (tempBuffer[0].len <= 0)
	{
		MMOGameLib::Crash();
	}

#if IOCOUNT_CHECK ==1
	InterlockedIncrement(&g_IOPostCount);
	InterlockedIncrement(&g_IOIncDecCount);
#endif



#if MEMORYLOG_USE ==1 
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::ACCEPT_BEFORE_RECV, GetCurrentThreadId(), newSession->_Socket, newSession->_IOCount, (int64_t)newSession, newSession->_ID, (int64_t)&newSession->_RecvOL, (int64_t)&newSession->_SendOL, newSession->_SendFlag);
	g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::ACCEPT_BEFORE_RECV, GetCurrentThreadId(), newSession->_Socket, newSession->_IOCount, (int64_t)newSession, newSession->_ID, (int64_t)&newSession->_RecvOL, (int64_t)&newSession->_SendOL, newSession->_SendFlag);
	g_MemoryLog_IOCP.MemoryLogging(log);
	newSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif


	int recvRtn = WSARecv(newSession->_Socket, tempBuffer, 1, NULL, &flag, &newSession->_RecvOL, NULL);

	newSession->_SessionStatus = eSessionStatus::AUTH;

	//g_MemoryLog_IOCount.MemoryLogging(ACCEPT_RECVRTN, GetCurrentThreadId(), newSession->_ID, newSession->_IOCount, (int64_t)newSession->_Socket, (int64_t)newSession, (int64_t)&newSession->_RecvOL, (int64_t)&newSession->_SendOL);
	if (recvRtn == SOCKET_ERROR)
	{
		int errorCode = WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			newSession->_IOFail = true;

			DWORD tempIOCount = InterlockedDecrement(&newSession->_IOCount);

#if MEMORYLOG_USE ==1 
			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::ACCEPT_AFTER_RECV, GetCurrentThreadId(), newSession->_Socket, newSession->_IOCount, (int64_t)newSession, newSession->_ID, (int64_t)&newSession->_RecvOL, (int64_t)&newSession->_SendOL, newSession->_SendFlag);
			g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::ACCEPT_AFTER_RECV, GetCurrentThreadId(), newSession->_Socket, newSession->_IOCount, (int64_t)newSession, newSession->_ID, (int64_t)&newSession->_RecvOL, (int64_t)&newSession->_SendOL, newSession->_SendFlag);
			g_MemoryLog_IOCP.MemoryLogging(log);
			newSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif

			SpecialErrorCodeCheck(errorCode);
			if (0 == tempIOCount)
			{
				//MMOGameLib::Crash();
				newSession->_bReleaseReady = true;
			}
		}
	}
}
bool MMOGameLib::RecvPacket(MMOSession* curSession, DWORD transferByte)
{
	//-----------------------------------------
	// Enqueue Ȯ��
	//-----------------------------------------
	if ((int)transferByte > curSession->_RecvRingQ.GetFreeSize())
	{
#if DISCONLOG_USE ==1
		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"RecvRingQ �ʰ������� ���� [Session ID:%llu] [transferByte:%d]", curSession->_ID, transferByte);
#endif
		curSession->Disconnect();

		return false;
	}
	curSession->_RecvRingQ.MoveRear(transferByte);

	curSession->_LastRecvTime = timeGetTime();


	InterlockedAdd(&m_NetworkRecvTraffic, transferByte + 40);

	while (true)
	{
		NetHeader header;
		NetPacket* packet;

		int usedSize = curSession->_RecvRingQ.GetUsedSize();

		if (usedSize < sizeof(NetHeader))
		{
			break;
		}
		int peekRtn = curSession->_RecvRingQ.Peek((char*)&header, sizeof(NetHeader));

		if (header._Code != dfPACKET_CODE)
		{
			//----------------------------------------
			// ����� �ڵ尡 �ٸ� ��� ������ ���´�.
			//----------------------------------------
#if DISCONLOG_USE ==1
			_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"��� �ڵ尡 �ٸ� [Session ID:%llu] [Code:%d]", curSession->_ID, header._Code);
			_LOG->WriteLogHex(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Recv RingQ Hex", (BYTE*)curSession->_RecvRingQ.GetFrontBufferPtr(), curSession->_RecvRingQ.GetDirectDequeueSize());
#endif
			curSession->Disconnect();
			return false;
		}
		if (header._Len <= 0)
		{
			//----------------------------------------
			// ����ȿ� ǥ��� Len�� 0���� ���ų� ������ ���� ���´�.
			//----------------------------------------
#if DISCONLOG_USE ==1
			_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"����� Len :0  [Session ID:%llu] [Code:%d]", curSession->_ID, header._Len);
#endif
			curSession->Disconnect();

			return false;
		}
		if (usedSize - sizeof(NetHeader) < header._Len)
		{
			//-------------------------------------
			// ���������ϴ���Ŷ��, �� ������ ���� ����������� ũ�� ���̾ȵǱ⶧����,
			// �׷� Session�� ������ ���´�.
			//-------------------------------------
			if (header._Len > curSession->_RecvRingQ.GetFreeSize())
			{
#if DISCONLOG_USE ==1
				_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"RingQ������� ū ��Ŷ�̵��� [Session ID:%llu] [Header Len:%d] [My RingQ FreeSize:%d]", curSession->_ID, header._Len, curSession->_RecvRingQ.GetFreeSize());
#endif
				curSession->Disconnect();
				return false;
			}
			break;
		}
		curSession->_RecvRingQ.MoveFront(sizeof(header));


		packet = NetPacket::Alloc();
		InterlockedIncrement(&g_AllocMemoryCount);

		int deQRtn = curSession->_RecvRingQ.Dequeue((*packet).GetPayloadPtr(), header._Len);


		if (deQRtn != header._Len)
		{
			MMOGameLib::Crash();
		}
		(*packet).MoveWritePos(deQRtn);

		if ((*packet).GetPayloadSize() == 0)
		{
			MMOGameLib::Crash();
		}

		if (!packet->Decoding(&header))
		{
			//-----------------------------------
			// Decoding ���н�, ������ ���´�.
			//-----------------------------------
#if DISCONLOG_USE ==1
			_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Decoding ���� [Session ID:%llu] ", curSession->_ID);
#endif
			InterlockedIncrement(&g_FreeMemoryCount);
			packet->Free(packet);
			curSession->Disconnect();
			return false;
		}
		InterlockedIncrement((LONG*)&m_RecvTPS);

		while (!curSession->_CompleteRecvPacketQ.Enqueue(packet))
		{
			
		}

		//curSession->OnRecv(packet);

	}

	RecvPost(curSession);

	return true;
}
bool MMOGameLib::RecvPost(MMOSession* curSession)
{
#if MEMORYLOG_USE ==1 ||   MEMORYLOG_USE ==2
	IOCP_Log log;
#endif

	//SendFlag_Log sendFlagLog;
	//-------------------------------------------------------------
	// Recv �ɱ�
	//-------------------------------------------------------------
	if (curSession->_RecvRingQ.GetFreeSize() <= 0)
	{
#if DISCONLOG_USE ==1
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"RecvPost��, ������ ������������� [Session ID:%llu] ", curSession->_ID);
#endif
#if MEMORYLOG_USE ==1 
		log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::RECVPOST_FREESIZE_NON, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
		g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
		log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::RECVPOST_FREESIZE_NON, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
		g_MemoryLog_IOCP.MemoryLogging(log);
		curSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif

		curSession->Disconnect();
		return false;
	}
	if (curSession->_IOCount <= 0)
	{
		MMOGameLib::Crash();
	}

	//sendFlagLog.DataSettiong(InterlockedIncrement64(&m_SendFlagNo), eSendFlag::RECVPOST, GetCurrentThreadId(), curSession->_Socket, (int64_t)curSession, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag, curSession->_SendQ.GetQCount());
	//curSession->_MemoryLog_SendFlag.MemoryLogging(sendFlagLog);

	DirectData directData;
	int bufCount = 0;

	curSession->_RecvRingQ.GetDirectEnQData(&directData);

	WSABUF wsaRecvBuf[2];

	wsaRecvBuf[0].buf = directData.bufferPtr1;
	wsaRecvBuf[0].len = directData._Direct1;
	bufCount = 1;

	if (directData._Direct2 != 0)
	{
		bufCount = 2;
		wsaRecvBuf[1].buf = directData.bufferPtr2;
		wsaRecvBuf[1].len = directData._Direct2;
	}

	if (directData._Direct1 <0 || directData._Direct2<0 || directData._Direct1> RingQ::RING_BUFFER_SIZE || directData._Direct2> RingQ::RING_BUFFER_SIZE)
	{
		MMOGameLib::Crash();
	}
	DWORD flag = 0;

	if (curSession->_IOCount <= 0)
	{
		MMOGameLib::Crash();
	}

	InterlockedIncrement(&curSession->_IOCount);
#if IOCOUNT_CHECK ==1
	InterlockedIncrement(&g_IOPostCount);
	InterlockedIncrement(&g_IOIncDecCount);
#endif


#if MEMORYLOG_USE ==1 
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::RECVPOST_BEFORE_RECV, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
	g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::RECVPOST_BEFORE_RECV, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
	g_MemoryLog_IOCP.MemoryLogging(log);
	curSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif


	ZeroMemory(&curSession->_RecvOL, sizeof(curSession->_RecvOL));

	if (curSession->_IOCount <= 0)
	{

		MMOGameLib::Crash();
	}

	//------------------------------------------------------------------
	// 	   IO Cancel �� ������ٸ�, ������� �����ʰ�, IOCount�� ���߰� Return�Ѵ�
	// 
	//------------------------------------------------------------------
	if (curSession->_bIOCancel)
	{
		if (0 == InterlockedDecrement(&curSession->_IOCount))
		{
			curSession->_bReleaseReady = true;
		}

		return false;
	}
	int recvRtn = WSARecv(curSession->_Socket, wsaRecvBuf, bufCount, NULL, &flag, &curSession->_RecvOL, NULL);
	if (curSession->_IOCount <= 0)
	{
		MMOGameLib::Crash();
	}
	//g_MemoryLog_IOCount.MemoryLogging(RECVPOST_RECVRTN, GetCurrentThreadId(), curSession->_ID, curSession->_IOCount, (int64_t)curSession->_Socket, (int64_t)curSession, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL);

	if (recvRtn == SOCKET_ERROR)
	{
		int errorCode = WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			curSession->_IOFail = true;
			int tempIOCount = InterlockedDecrement(&curSession->_IOCount);

#if MEMORYLOG_USE ==1 
			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::RECVPOST_AFTER_RECV, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
			g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::RECVPOST_AFTER_RECV, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
			g_MemoryLog_IOCP.MemoryLogging(log);
			curSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif
			SpecialErrorCodeCheck(errorCode);
			if (0 == tempIOCount)
			{
				MMOGameLib::Crash();
				curSession->_bReleaseReady = true;
			}
			if (tempIOCount < 0)
			{
				MMOGameLib::Crash();
			}
			return false;
		}
	}
	return true;
}





int64_t MMOGameLib::GetAcceptCount()
{
	return m_AcceptCount;
}

LONG MMOGameLib::GetAcceptTPS()
{
	return m_AcceptTPS_To_Main;
}

LONG MMOGameLib::GetSendTPS()
{
	return m_SendTPS_To_Main;
}

LONG MMOGameLib::GetRecvTPS()
{
	return m_RecvTPS_To_Main;
}

LONG MMOGameLib::GetNetworkTraffic()
{
	return m_NetworkTraffic_To_Main;
}

LONG MMOGameLib::GetNetworkRecvTraffic()
{
	return m_NetworkRecvTraffic_To_Main;
}

LONG MMOGameLib::GetSessionCount()
{
	return m_SessionCount;
}

int32_t MMOGameLib::GetMemoryAllocCount()
{
	return NetPacket::GetMemoryPoolAllocCount();
}

LONG MMOGameLib::GetSendQMeomryCount()
{
	return m_SendQMemory;
}

LONG MMOGameLib::GetLockFreeStackMemoryCount()
{
	if (m_IndexStack == nullptr)
	{
		return 0;
	}
	return m_IndexStack->GetMemoryAllocCount();
}

DWORD MMOGameLib::GetAuthFPS()
{
	return m_AuthFPS_To_Main;
}

DWORD MMOGameLib::GetGameFPS()
{
	return m_GameFPS_To_Main;
}

DWORD MMOGameLib::GetSendFPS()
{
	return m_SendFPS_To_Main;
}


void MMOGameLib::SetSessionArray(MMOSession* sessionArray,DWORD maxUserCount)
{
	for (int i = 0; i < maxUserCount; ++i)
	{

		m_SessionArray[i] = sessionArray+i;

	}
}
unsigned int __stdcall MMOGameLib::AcceptThread(LPVOID param)
{
	MMOGameLib* mmoServer = (MMOGameLib*)param;

	while (true)
	{
		SOCKADDR_IN clientAddr;
		ZeroMemory(&clientAddr, sizeof(clientAddr));
		int addrLen = sizeof(clientAddr);
		SOCKET clientSocket;

		clientSocket = accept(mmoServer->m_ListenSocket, (sockaddr*)&clientAddr, &addrLen);
		if (clientSocket == INVALID_SOCKET)
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"accept () error:%d", WSAGetLastError());
			break;
		}
		mmoServer->m_AcceptCount++;

		WCHAR tempIP[17] = { 0, };
		uint16_t tempPort = ntohs(clientAddr.sin_port);
		InetNtop(AF_INET, &clientAddr.sin_addr.S_un.S_addr, tempIP, 17);

		////----------------------------------------------------//
		////Black IP ���� �� White IP ���ǻ��� 
		////----------------------------------------------------//
		//if (!mmoServer->m_Contents->OnConnectionRequest(tempIP, tempPort))
		//{
		//	closesocket(clientSocket);
		//	continue;
		//}
		mmoServer->AcceptUser(clientSocket, tempIP, tempPort);
		mmoServer->m_AcceptTPS++;

	}

	_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"Accpt Thread[%d] ����", GetCurrentThreadId());
	return 0;
}

unsigned int __stdcall MMOGameLib::WorkerThread(LPVOID param)
{	
	MMOGameLib* mmoServer = (MMOGameLib*)param;

	while (true)
	{
		DWORD transferByte = 0;
		OVERLAPPED* curOverlap = nullptr;
		MMOSession* curSession = nullptr;
		BOOL gqcsRtn = GetQueuedCompletionStatus(mmoServer->m_IOCP, &transferByte, (PULONG_PTR)&curSession, (LPOVERLAPPED*)&curOverlap, INFINITE);

		int errorCode;

		if (gqcsRtn == FALSE)
		{
			errorCode = WSAGetLastError();
			curSession->_ErrorCode = errorCode;
		}

#if MEMORYLOG_USE ==1 ||   MEMORYLOG_USE ==2
		IOCP_Log log;
#endif
		//----------------------------------------------
		// GQCS���� ���� �������� nullptr�̸�, Ÿ�Ӿƿ�, IOCP��ü�� ���� , �ƴϸ� postQ�� NULL�� �־������̴�
		//----------------------------------------------
		if (curOverlap == NULL && curSession == NULL && transferByte == 0)
		{
			break;
		}
		do
		{
			if (curSession == nullptr)
			{
				mmoServer->MMOGameLib::Crash();
			}
			if (curSession->_IOCount == 0)
			{
				mmoServer->MMOGameLib::Crash();
			}
			curSession->_GQCSRtn = gqcsRtn;
#if MEMORYLOG_USE ==1 
			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::GQCS, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
			g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::GQCS, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
			g_MemoryLog_IOCP.MemoryLogging(log);
			curSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif

			uint64_t sessionID = curSession->_ID;

			if (transferByte == 0)
			{
				if (curOverlap == &curSession->_RecvOL)
				{

					if (gqcsRtn == TRUE)
					{
						mmoServer->MMOGameLib::Crash();
					}
					curSession->_TransferZero = 5;
#if MEMORYLOG_USE ==1 
					log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::TRANSFER_ZERO_RECV, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, sessionID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
					g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
					log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::TRANSFER_ZERO_RECV, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
					g_MemoryLog_IOCP.MemoryLogging(log);
					curSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif
				}
				else if (curOverlap == &curSession->_SendOL)
				{
					if (gqcsRtn == TRUE)
					{
						mmoServer->MMOGameLib::Crash();
					}
					curSession->_TransferZero = 6;
#if MEMORYLOG_USE ==1 
					log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::TRANSFER_ZERO_SEND, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, sessionID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
					g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
					log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::TRANSFER_ZERO_SEND, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
					g_MemoryLog_IOCP.MemoryLogging(log);
					curSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif
				}
				//----------------------------------------------
				// �۾� ���н� close socket�� ���ش�
				//----------------------------------------------
				break;
			}

			if (curOverlap == &curSession->_RecvOL)
			{

#if MEMORYLOG_USE ==1 
				log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::RECV_COMPLETE, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, sessionID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
				g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
				log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::RECV_COMPLETE, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
				g_MemoryLog_IOCP.MemoryLogging(log);
				curSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif

				if (curSession->_IOCount <= 0)
				{
					mmoServer->MMOGameLib::Crash();
				}
				mmoServer->RecvPacket(curSession, transferByte);
			}
			else if (curOverlap == &curSession->_SendOL)
			{
				/*		if (curSession->_SendFlag != 1)
						{
							MMOGameLib::Crash();
						}*/


#if MEMORYLOG_USE ==1 
				log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::SEND_COMPLETE, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, sessionID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
				g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
				log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::SEND_COMPLETE, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
				g_MemoryLog_IOCP.MemoryLogging(log);
				curSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif
				if (curSession->_IOCount <= 0)
				{
					mmoServer->MMOGameLib::Crash();
				}

				if (curSession->_SendByte != transferByte)
				{
					mmoServer->MMOGameLib::Crash();
				}

				mmoServer->DeQPacket(curSession);
				curSession->_SendByte = 0;

				////-------------------------------------------------------
				//// send �Ϸ������� �Ա⶧���� SendFlag�� �ٲ��ش�. 
				////-------------------------------------------------------
				InterlockedExchange(&curSession->_SendFlag, 0);

				InterlockedAdd(&mmoServer->m_NetworkTraffic, transferByte + 40);


			}
			else
			{
				// ������ ���°ź�
				//--------------------------------
				// For Devbug
				//--------------------------------
				mmoServer->m_MemoryLog_Overlap.MemoryLogging(ELSE_OVERLAP, GetCurrentThreadId(), curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, (int64_t)curOverlap, (int64_t)curSession->_Socket, (int64_t)curSession);
				mmoServer->MMOGameLib::Crash();
			}

		} while (0);


		if (curSession->_IOCount <= 0)
		{
			mmoServer->MMOGameLib::Crash();
		}

		//-------------------------------------------------------------
		// �Ϸ������� ���� IO ����
		//-------------------------------------------------------------

#if MEMORYLOG_USE ==1 
		log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::LAST_COMPLETE, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
		g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
		log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::LAST_COMPLETE, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
		g_MemoryLog_IOCP.MemoryLogging(log);
		curSession->_MemoryLog_IOCP.MemoryLogging(log);
#endif

		int tempIOCount = InterlockedDecrement(&curSession->_IOCount);
#if IOCOUNT_CHECK ==1
		InterlockedIncrement(&g_IOCompleteCount);
		InterlockedDecrement(&g_IOIncDecCount);
#endif
		if (0 == tempIOCount)
		{
			curSession->_bReleaseReady = true;
		}

	}
	_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"WorkerThread[%d] ����", GetCurrentThreadId());

	return 0;
}

unsigned int __stdcall MMOGameLib::MonitorThread(LPVOID param)
{

	MMOGameLib* mmoServer = (MMOGameLib*)param;
	DWORD maxUserCount = mmoServer->m_MaxUserCount;
	MMOSession** sessionArray = mmoServer->m_SessionArray;


	while (true)
	{
		DWORD rtn = WaitForSingleObject(mmoServer->m_MonitorEvent, 999);
		if (rtn != WAIT_TIMEOUT)
		{
			break;
		}

		time_t dataTime;

		time(&dataTime);

		//mmoServer->m_MonitorClient->SendPacket(m_MonitorClient)

		//-------------------------------------------------------------
		// ��� ������ ������ť ��� �ջ�
		//-------------------------------------------------------------
		mmoServer->m_SendQMemory = 0;

		for (DWORD i = 0; i < mmoServer->m_MaxUserCount; ++i)
		{
			MMOSession* curSession = mmoServer->m_SessionArray[i];

			mmoServer->m_SendQMemory += curSession->_SendQ.GetMemoryPoolAllocCount();
		}


		mmoServer->m_RecvTPS_To_Main = mmoServer->m_RecvTPS;
		mmoServer->m_SendTPS_To_Main = mmoServer->m_SendTPS;
		mmoServer->m_AcceptTPS_To_Main = mmoServer->m_AcceptTPS;
		mmoServer->m_NetworkTraffic_To_Main = mmoServer->m_NetworkTraffic;
		mmoServer->m_NetworkRecvTraffic_To_Main = mmoServer->m_NetworkRecvTraffic;


		InterlockedExchange(&mmoServer->m_RecvTPS, 0);
		InterlockedExchange(&mmoServer->m_SendTPS, 0);
		InterlockedExchange(&mmoServer->m_AcceptTPS, 0);
		InterlockedExchange(&mmoServer->m_NetworkTraffic, 0);
		InterlockedExchange(&mmoServer->m_NetworkRecvTraffic, 0);
	}

	_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"MonitorThread ����");

	return 0;
}

unsigned int __stdcall MMOGameLib::GameLogicThread(LPVOID param)
{
	MMOGameLib* mmoServer = (MMOGameLib*)param;
	DWORD maxUserCount = mmoServer->m_MaxUserCount;
	MMOSession** sessionArray = mmoServer->m_SessionArray;
	int fpsCount = 0;

	DWORD time = timeGetTime();
	while (true)
	{
		DWORD  rtnWait = WaitForSingleObject(mmoServer->m_GameThreadEvent, 5);

		if (rtnWait == WAIT_FAILED)
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Wait Fail GameLogicThread:%d", GetCurrentThreadId());
			mmoServer->MMOGameLib::Crash();
		}
		//-----------------------------------------------------
		// ���� ��ȣ ������,  Game Logic  ����������
		//-----------------------------------------------------
		if (rtnWait != WAIT_TIMEOUT)
		{
			break;
		}

		fpsCount++;

		if (timeGetTime() - time > 1000)
		{
			mmoServer->m_GameFPS_To_Main = fpsCount;
			fpsCount = 0;
			time = timeGetTime();
		}

		for (DWORD i = 0; i < maxUserCount; ++i)
		{
			MMOSession* curSession = sessionArray[i];

			if (curSession->_SessionStatus == eSessionStatus::AUTH_TO_GAME)
			{
				//----------------------------------------------
				// Auth ���� Game���� �Ѿ�� �ֵ��� �ʱ�ȭ (�����Ҵ�,..��Ÿ���)�������� Game���·� �ٲ۴�
				//----------------------------------------------
				curSession->_SessionStatus = eSessionStatus::GAME;

				mmoServer->m_GameSessionCount++;

			}

			if (curSession->_SessionStatus == eSessionStatus::GAME)
			{
				if (curSession->_bReleaseReady)
				{
					curSession->_SessionStatus = eSessionStatus::GAME_RELEASE;
				}
				else
				{
					
					//wprintf(L"QCount:%d", curSession->_CompleteRecvPacketQ.GetUsedSize());
					while (true)
					{
						NetPacket* packet = nullptr;
					
						if (curSession->_CompleteRecvPacketQ.Dequeue(&packet) == true)
						{
							curSession->OnGamePacket(packet);
						}
						else
						{
							break;
						}
					}
					
				}
			}
			
			if (curSession->_SessionStatus == eSessionStatus::GAME_RELEASE && curSession->_bSending == false)
			{
				curSession->_SessionStatus = eSessionStatus::RELEASE;
			}

			if (curSession->_SessionStatus == eSessionStatus::RELEASE)
			{
				//-----------------------------------------------------
				// Game�����忡���� Release�� �����Ѵ�.
				// ReleaseSession ���ο��� ���¸� Release -> Not Used �� �ٲ�
				//-----------------------------------------------------
				mmoServer->ReleaseSession(curSession);
				mmoServer->m_GameSessionCount--;

			}

		}


	}
	return 0;
}

unsigned int __stdcall MMOGameLib::AuthThread(LPVOID param)
{
	MMOGameLib* mmoServer = (MMOGameLib*)param;
	DWORD maxUserCount = mmoServer->m_MaxUserCount;
	MMOSession** sessionArray = mmoServer->m_SessionArray;
	int fpsCount = 0;

	DWORD time = timeGetTime();

	while (true)
	{
		DWORD  rtnWait = WaitForSingleObject(mmoServer->m_AuthTreadEvent, 10);


		if (timeGetTime() - time > 1000)
		{
			mmoServer->m_AuthFPS_To_Main = fpsCount;
			fpsCount = 0;
			time = timeGetTime();
		}

		if (rtnWait == WAIT_FAILED)
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Wait Fail AuthThread:%d", GetCurrentThreadId());
			mmoServer->MMOGameLib::Crash();
		}
		//-----------------------------------------------------
		// ���� ��ȣ ������,  AuthThread ����������
		//-----------------------------------------------------
		if (rtnWait  != WAIT_TIMEOUT)
		{
			break;
		}

		++fpsCount;

		ConnectInfo* conInfo;


		//-------------------------------------------------------------
		// ���������� ->  Auth����
		//-------------------------------------------------------------

		while (mmoServer->m_SocketQ.Dequeue(&conInfo))
		{
			//-----------------------------------------------------
			// ReleaseSession ���ο��� ���¸� NotUsed->Auth ���·ιٲ�.
			//-----------------------------------------------------
			mmoServer->CreateNewSession(conInfo);
			++mmoServer->m_AuthSessionCount;

		}

		//-------------------------------------------------------------
		// Auth���� -> ��Ŷó��, Release ó��
		//-------------------------------------------------------------
		for (DWORD i = 0; i < maxUserCount; ++i)
		{
			MMOSession* curSession = sessionArray[i];

			if (curSession->_SessionStatus == eSessionStatus::AUTH)
			{	
				if (curSession->_bReleaseReady)
				{
					curSession->_SessionStatus = eSessionStatus::AUTH_RELEASE;
				}
				else
				{
					NetPacket* packet;

					if (curSession->_CompleteRecvPacketQ.Dequeue(&packet))
					{
						curSession->OnAuthPacket(packet);
						//-------------------------------------------------
						// OnAuthPacket���� �α�����Ŷ�� �ð��̹Ƿ�, ���⼭ DBó����,
						// Auth_To_Game���·� �ٲ۴�.
						//-------------------------------------------------
						curSession->_SessionStatus = eSessionStatus::AUTH_TO_GAME;
						--mmoServer->m_AuthSessionCount;

					}
				}
			}

			if (curSession->_SessionStatus == eSessionStatus::AUTH_RELEASE && curSession->_bSending == false)
			{
				curSession->_SessionStatus = eSessionStatus::RELEASE;
				--mmoServer->m_AuthSessionCount;
			}
			
		}
	}
	return 0;
}

unsigned int __stdcall MMOGameLib::SendThread(LPVOID param)
{
	MMOGameLib* mmoServer = (MMOGameLib*)param;
	DWORD maxUserCount = mmoServer->m_MaxUserCount;
	MMOSession** sessionArray = mmoServer->m_SessionArray;
	int fpsCount = 0;
	DWORD time = timeGetTime();

	while (true)
	{
		DWORD  rtnWait = WaitForSingleObject(mmoServer->m_SendThreadEvent, 4);

		if (rtnWait == WAIT_FAILED)
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Wait Fail SendThread:%d", GetCurrentThreadId());
			mmoServer->MMOGameLib::Crash();
		}
		//-----------------------------------------------------
		// ���� ��ȣ ������,  SendThread ����������
		//-----------------------------------------------------
		if (rtnWait != WAIT_TIMEOUT)
		{
			break;
		}
		++fpsCount;

		if (timeGetTime() - time > 1000)
		{
			mmoServer->m_SendFPS_To_Main = fpsCount;
			fpsCount = 0;
			time = timeGetTime();
		}

		for (DWORD i = 0; i < maxUserCount; ++i)
		{
			MMOSession* curSession = sessionArray[i];
			//uint64_t  tempID = curSession->_ID;

			curSession->_bSending = true;

			if (curSession->_SessionStatus == eSessionStatus::AUTH || curSession->_SessionStatus == eSessionStatus::GAME)
			{				
				curSession->SendPost();
			}
			else 
			{
				curSession->_bSending = false;
			}

		}
	}
	return 0;
}

void MMOGameLib::Crash()
{
	int* p = nullptr;
	*p = 10;
}

bool MMOSession::Disconnect()
{
#if MEMORYLOG_USE ==1 
	IOCP_Log log;
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::DISCONNECT_CLIENT, GetCurrentThreadId(), _Socket, _IOCount, (int64_t)this, _ID, (int64_t)&_RecvOL, (int64_t)&_SendOL, _SendFlag);
	g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
	IOCP_Log log;
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::DISCONNECT_CLIENT, GetCurrentThreadId(), _Socket, _IOCount, (int64_t)this, _ID, (int64_t)&_RecvOL, (int64_t)&_SendOL, _SendFlag);
	g_MemoryLog_IOCP.MemoryLogging(log);
	_MemoryLog_IOCP.MemoryLogging(log);
#endif

	InterlockedIncrement(&g_DisconnectCount);

	_bIOCancel = true;
	IO_Cancel();

	return true;
}

void MMOSession::IO_Cancel()
{
	//-----------------------------------------------
	// Overlapped Pointer�� NULL�Ͻ� Send ,Recv �Ѵ� IO����Ѵ�
	//-----------------------------------------------
	CancelIoEx((HANDLE)_Socket, NULL);
}

bool MMOSession::SendPost()
{
	int loopCount = 0;

	//sendFlagLog.DataSettiong(InterlockedIncrement64(&m_SendFlagNo), eSendFlag::SENDPOST_ENTRY, GetCurrentThreadId(), curSession->_Socket, (int64_t)curSession, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag, curSession->_SendQ.GetQCount());
	//curSession->_MemoryLog_SendFlag.MemoryLogging(sendFlagLog);

	do
	{
		loopCount++;

		if (0 == InterlockedExchange(&_SendFlag, 1))
		{
			//--------------------------------------------------------
			// Echo Count�� ������ ����
			//--------------------------------------------------------
			if (_SendQ.GetQCount() <= 0)
			{
				InterlockedExchange(&_SendFlag, 0);
				continue;
			}
			//--------------------------------------------------
			// IOCount�� �̼����� WSARecv or WSASend ���� �α׸� ���� Session�� �����Ҽ��ֱ� ������
			// ����ī��Ʈ������ �ϳ� �� ������Ų��.
			//--------------------------------------------------
			InterlockedAdd((LONG*)&_IOCount, 2);
			//--------------------------------------------------

			if (_IOCount <= 0)
			{
				MMOGameLib::Crash();
			}

			if (_SessionStatus == eSessionStatus::RELEASE)
			{
				Crash();
			}

#if MEMORYLOG_USE ==1 ||   MEMORYLOG_USE ==2
			IOCP_Log log;
#endif

#if MEMORYLOG_USE ==1 
			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::SENDPOST_BEFORE_SEND, GetCurrentThreadId(), _Socket, _IOCount, (int64_t)this, _ID, (int64_t)&_RecvOL, (int64_t)&_SendOL, _SendFlag);
			g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::SENDPOST_BEFORE_SEND, GetCurrentThreadId(), _Socket, _IOCount, (int64_t)this, _ID, (int64_t)&_RecvOL, (int64_t)&_SendOL, _SendFlag);
			g_MemoryLog_IOCP.MemoryLogging(log);
			_MemoryLog_IOCP.MemoryLogging(log);
#endif
			////-----------------------------------------------------------------------------------------------------------------------
			//// SendQ�� �ִ� LanPackt* �����͵��� �̾Ƽ� WSABUF�� �������ش�
			////-----------------------------------------------------------------------------------------------------------------------
			WSABUF wsaSendBuf[MMOSession::DEQ_PACKET_ARRAY_SIZE];

			int bufCount = 0;

			NetPacket* deQPacket = nullptr;

			if (_DeQArraySize > 0)
			{
				MMOGameLib::Crash();

			}
			while (_SendQ.DeQ(&deQPacket))
			{
				if (deQPacket == nullptr)
				{
					MMOGameLib::Crash();
				}
				if (_DeQArraySize > MMOSession::DEQ_PACKET_ARRAY_SIZE - 1)
				{
					MMOGameLib::Crash();
				}
				////---------------------------------------------------------------
				//// For Debug
				////---------------------------------------------------------------
				//NetHeader tempHeader;
				//memcpy(&tempHeader, deQPacket->GetBufferPtr(), sizeof(NetHeader));

				//if (tempHeader._Code != dfPACKET_CODE)
				//{
				//	MMOGameLib::Crash();
				//}

				if (deQPacket->GetPayloadSize() <= 0)
				{
					MMOGameLib::Crash();
				}
				wsaSendBuf[_DeQArraySize].buf = deQPacket->GetBufferPtr();
				wsaSendBuf[_DeQArraySize].len = deQPacket->GetFullPacketSize();
				_SendByte += wsaSendBuf[_DeQArraySize].len;

				_DeQPacketArray[_DeQArraySize] = deQPacket;
				_DeQArraySize++;
			}
			//------------------------------------------------------
			//   Send �۽Ź���Ʈ üũ�ϱ�
			//------------------------------------------------------
			if (_SendByte <= 0)
			{
				MMOGameLib::Crash();
			}

			ZeroMemory(&_SendOL, sizeof(_SendOL));

			if (_IOCount <= 0)
			{
				MMOGameLib::Crash();
			}

			//------------------------------------------------------------------
			// 	IO Cancel �� ������ٸ�, ������� �����ʰ�, IOCount�� ���߰� Return�Ѵ�
			//  �α׸����� IOCount +1  WSASend�� ���� +1 
			//------------------------------------------------------------------
			if (_bIOCancel)
			{
				for (int i = 0; i < 2; ++i)
				{
					if (0 == InterlockedDecrement(&_IOCount))
					{
						_bReleaseReady = true;
					}
				}
				
				return false;
			}


			int sendRtn = WSASend(_Socket, wsaSendBuf, _DeQArraySize, NULL, 0, &_SendOL, NULL);

#if MEMORYLOG_USE ==1 
			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::SENDPOST_SENDRTN, GetCurrentThreadId(), _Socket, _IOCount, (int64_t)this, _ID, (int64_t)&_RecvOL, (int64_t)&_SendOL, _SendFlag);
			g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::SENDPOST_SENDRTN, GetCurrentThreadId(), _Socket, _IOCount, (int64_t)this, _ID, (int64_t)&_RecvOL, (int64_t)&_SendOL, _SendFlag);
			g_MemoryLog_IOCP.MemoryLogging(log);
			_MemoryLog_IOCP.MemoryLogging(log);
#endif

			//sendFlagLog.DataSettiong(InterlockedIncrement64(&m_SendFlagNo), eSendFlag::AFTER_SEND, GetCurrentThreadId(), curSession->_Socket, (int64_t)curSession, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag, curSession->_SendQ.GetQCount());
			//curSession->_MemoryLog_SendFlag.MemoryLogging(sendFlagLog);


			if (_IOCount <= 0)
			{
				MMOGameLib::Crash();
			}

			if (sendRtn == SOCKET_ERROR)
			{
				int errorCode = WSAGetLastError();

				if (errorCode != WSA_IO_PENDING)
				{
					_IOFail = true;
					MMOGameLib::SpecialErrorCodeCheck(errorCode);

					//---------------------------------------------------------
					// WSASend�� �ɱ����� ������Ų IOCount�� ���ҽ�Ų��.
					//---------------------------------------------------------
					int tempIOCount = InterlockedDecrement(&_IOCount);

#if MEMORYLOG_USE ==1 
					log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::SENDPOST_AFTER_SEND, GetCurrentThreadId(), _Socket, _IOCount, (int64_t)this, _ID, (int64_t)&_RecvOL, (int64_t)&_SendOL, _SendFlag);
					g_MemoryLog_IOCP.MemoryLogging(log);
#endif
#if MEMORYLOG_USE  ==2
					log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::SENDPOST_AFTER_SEND, GetCurrentThreadId(), _Socket, _IOCount, (int64_t)this, _ID, (int64_t)&_RecvOL, (int64_t)&_SendOL, _SendFlag);
					g_MemoryLog_IOCP.MemoryLogging(log);
					_MemoryLog_IOCP.MemoryLogging(log);
#endif

					if (0 == tempIOCount)
					{
						_bReleaseReady = true;
					}

				}
				else
				{
					/*	sendFlagLog.DataSettiong(InterlockedIncrement64(&m_SendFlagNo), eSendFlag::IO_PENDING, GetCurrentThreadId(), curSession->_Socket, (int64_t)curSession, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag, curSession->_SendQ.GetQCount());
						curSession->_MemoryLog_SendFlag.MemoryLogging(sendFlagLog);*/
				}
			}

			//---------------------------------------------------------
			// Log�� ���� �÷Ǵ� IOCount�� ���ҽ�Ű�� ������. (Return)
			//---------------------------------------------------------
			int tempIOCount = InterlockedDecrement(&_IOCount);
			if (0 == tempIOCount)
			{
				_bReleaseReady = true;
			}
			return true;
		}
		else
		{
			break;
		}

	
	} while (_SendQ.GetQCount() > 0);

	return true;

}

bool MMOSession::SendPacket(NetPacket* packet)
{
#if MEMORYLOG_USE ==1 ||   MEMORYLOG_USE ==2
	IOCP_Log log;
#endif

#if MEMORYLOG_USE ==1 
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::SENDPACKET, GetCurrentThreadId(), _Socket, _IOCount, (int64_t)this, _ID, (int64_t)&_RecvOL, (int64_t)&_SendOL, _SendFlag);
	g_MemoryLog_IOCP.MemoryLogging(log);
#endif

#if MEMORYLOG_USE  ==2
	log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::SENDPACKET, GetCurrentThreadId(), _Socket, _IOCount, (int64_t)this, _ID, (int64_t)&_RecvOL, (int64_t)&_SendOL, _SendFlag);
	g_MemoryLog_IOCP.MemoryLogging(log);
	_MemoryLog_IOCP.MemoryLogging(log);
#endif

	(*packet).HeaderSettingAndEncoding();

	if (packet->GetPayloadSize() <= 0)
	{
		MMOGameLib::Crash();
	}
	if (!_SendQ.EnQ(packet))
	{
		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"SendQ �Ѱ��� �ʰ�(LockFreeQ Qcount �ʰ���)");
		MMOGameLib::Crash();
	}

	return true;
}

void MMOSession::SendUnicast(NetPacket* packet)
{
	packet->IncrementRefCount();

	if (!SendPacket(packet))
	{
		if (packet->DecrementRefCount() == 0)
		{
			InterlockedIncrement(&g_FreeMemoryCount);
			packet->Free(packet);
		}
		return;
	}

	if (packet->DecrementRefCount() == 0)
	{
		InterlockedIncrement(&g_FreeMemoryCount);
		packet->Free(packet);
	}
}




