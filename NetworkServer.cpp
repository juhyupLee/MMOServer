
#include "NetworkServer.h"

NetworkServer::NetworkServer()
	:m_ConInfoPool(500)
{
	setlocale(LC_ALL, "");

	m_SessionID = 0;

	m_AuthSessionCount = 0;
	m_GameSessionCount = 0;

	m_NetworkRecvTraffic = 0;
	m_NetworkTraffic = 0;

}

NetworkServer::~NetworkServer()
{
}
//
//bool NetworkServer::ServerStart(WCHAR* ip, uint16_t port, DWORD runningThread, SocketOption& option, DWORD workerThreadCount, DWORD maxUserCount, TimeOutOption& timeOutOption,NetworkSession** sessionArray)
//{
//	timeBeginPeriod(1);
//
//	if (!NetworkInit(ip, port, runningThread, option))
//	{
//		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"NetworkInit Fail");
//	}
//
//	//-----------------------------------------------------
//	// 세션의 Index를 관리할 락프리 스택 생성 
//	// ServerStop시, delete
//	//-----------------------------------------------------
//	m_TimeOutOption = timeOutOption;
//	m_IndexStack = new LockFreeStack<uint64_t>;
//
//	m_SessionArray = new NetworkSession*[maxUserCount];
//	m_MaxUserCount = maxUserCount;
//	for (uint64_t index = 0; index < m_MaxUserCount; ++index)
//	{
//		m_SessionArray[index] = sessionArray[index];
// 		m_IndexStack->Push(index);
//	}
//
//	EventInit();
//
//	ThreadInit(workerThreadCount);
//
//	if (0 != listen(m_ListenSocket, SOMAXCONN))
//	{
//		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"listen() error:%d", WSAGetLastError());
//		return false;
//	}
//	_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"IOCP Echo Server Listen......");
//	return true;
//}
//
//void NetworkServer::ServerStop()
//{
//	timeEndPeriod(1);
//	//-------------------------------------------------------------
//	// 	   Monitor 서버와 연결할 클라 리소스 정리
//	//-------------------------------------------------------------
//	// 
//
//	//-----------------------------------------------------------------------
//	// Accept Thread Resource 정리
//	//-----------------------------------------------------------------------
//	closesocket(m_ListenSocket);
//	DWORD rtn = WaitForSingleObject(m_AcceptThread, INFINITE);
//	CloseHandle(m_AcceptThread);
//
//	//-----------------------------------------------------------------------
//	// Monitor Thread Resource 정리
//	//-----------------------------------------------------------------------
//	SetEvent(m_MonitorEvent);
//	WaitForSingleObject(m_MonitoringThread, INFINITE);
//	CloseHandle(m_MonitoringThread);
//	CloseHandle(m_MonitorEvent);
//
//	if (DisconnectAllUser())
//	{
//		for (size_t i = 0; i < m_WorkerThreadCount; ++i)
//		{
//			PostQueuedCompletionStatus(m_IOCP, 0, NULL, NULL);
//			CloseHandle(m_WorkerThread[i]);
//		}
//		WaitForMultipleObjects(m_WorkerThreadCount, m_WorkerThread, TRUE, INFINITE);
//	}
//
//	//-----------------------------------------------------------------------
//	// Auth Thread Resource 정리
//	//-----------------------------------------------------------------------
//	SetEvent(m_AuthTreadEvent);
//	WaitForSingleObject(m_AuthThread, INFINITE);
//	CloseHandle(m_AuthTreadEvent);
//	CloseHandle(m_AuthThread);
//
//	//-----------------------------------------------------------------------
//	// Game Thread Resource 정리
//	//-----------------------------------------------------------------------
//	SetEvent(m_GameThreadEvent);
//	WaitForSingleObject(m_GameThread, INFINITE);
//	CloseHandle(m_GameThreadEvent);
//	CloseHandle(m_GameThread);
//
//	//-----------------------------------------------------------------------
//	// Send Thread Resource 정리
//	//-----------------------------------------------------------------------
//	SetEvent(m_SendThreadEvent);
//	WaitForSingleObject(m_SendThread, INFINITE);
//	CloseHandle(m_SendThreadEvent);
//	CloseHandle(m_SendThread);
//
//
//	_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"남은세션:%d", m_SessionCount);
//	_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"서버 종료");
//
//
//	delete m_IndexStack;
//	m_IndexStack = nullptr;
//
//	m_AcceptCount = 0;
//	m_AcceptTPS = 0;
//	m_RecvTPS = 0;
//	m_SendTPS = 0;
//	m_SendQMemory = 0;
//	m_AcceptTPS_To_Main = 0;
//	m_SendTPS_To_Main = 0;
//	m_RecvTPS_To_Main = 0;
//}
//
//bool NetworkServer::NetworkInit(WCHAR* ip, uint16_t port, DWORD runningThread, SocketOption option)
//{
//	WSAData wsaData;
//	SOCKADDR_IN serverAddr;
//
//	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
//	{
//		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"WSAStartUp() Error:%d", WSAGetLastError());
//		return false;
//	}
//	m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, runningThread);
//
//	m_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
//
//	if (m_ListenSocket == INVALID_SOCKET)
//	{
//		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"socket() error:%d", WSAGetLastError());
//		return false;
//	}
//
//	if (option._SendBufferZero)
//	{
//		//----------------------------------------------------------------------------
//		// 송신버퍼 Zero -->비동기 IO 유도
//		//----------------------------------------------------------------------------
//		int optVal = 0;
//		int optLen = sizeof(optVal);
//
//		int rtnOpt = setsockopt(m_ListenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&optVal, optLen);
//		if (rtnOpt != 0)
//		{
//			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
//		}
//	}
//	if (option._Linger)
//	{
//		linger lingerOpt;
//		lingerOpt.l_onoff = 1;
//		lingerOpt.l_linger = 0;
//
//		int rtnOpt = setsockopt(m_ListenSocket, SOL_SOCKET, SO_LINGER, (const char*)&lingerOpt, sizeof(lingerOpt));
//		if (rtnOpt != 0)
//		{
//			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
//		}
//
//	}
//	if (option._TCPNoDelay)
//	{
//		BOOL tcpNodelayOpt = true;
//
//		int rtnOpt = setsockopt(m_ListenSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&tcpNodelayOpt, sizeof(tcpNodelayOpt));
//		if (rtnOpt != 0)
//		{
//			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
//		}
//	}
//
//	if (option._KeepAliveOption.onoff)
//	{
//		DWORD recvByte = 0;
//
//		if (0 != WSAIoctl(m_ListenSocket, SIO_KEEPALIVE_VALS, &option._KeepAliveOption, sizeof(tcp_keepalive), NULL, 0, &recvByte, NULL, NULL))
//		{
//			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
//		}
//	}
//	ZeroMemory(&serverAddr, sizeof(SOCKADDR_IN));
//	serverAddr.sin_family = AF_INET;
//	serverAddr.sin_port = htons(port);
//
//	if (ip == nullptr)
//	{
//		serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
//	}
//	else
//	{
//		InetPton(AF_INET, ip, &serverAddr.sin_addr.S_un.S_addr);
//	}
//
//	if (0 != bind(m_ListenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
//	{
//		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"bind() error:%d", WSAGetLastError());
//		return false;
//	}
//
//	return true;
//}
//
//bool NetworkServer::EventInit()
//{
//	m_MonitorEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
//	m_GameThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
//	m_AuthTreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
//	m_SendThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
//
//	return true;
//}
//
//bool NetworkServer::ThreadInit(DWORD workerThreadCount)
//{
//	m_WorkerThreadCount = workerThreadCount;
//	m_WorkerThread = new HANDLE[workerThreadCount];
//
//	for (size_t i = 0; i < m_WorkerThreadCount; ++i)
//	{
//		m_WorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, NetworkServer::WorkerThread, this, 0, NULL);
//	}
//
//	m_AcceptThread = (HANDLE)_beginthreadex(NULL, 0, NetworkServer::AcceptThread, this, 0, NULL);
//	m_MonitoringThread = (HANDLE)_beginthreadex(NULL, 0, NetworkServer::MonitorThread, this, 0, NULL);
//	m_AuthThread = (HANDLE)_beginthreadex(NULL, 0, NetworkServer::AuthThread, this, 0, NULL);
//	m_GameThread = (HANDLE)_beginthreadex(NULL, 0, NetworkServer::GameLogicThread, this, 0, NULL);
//	m_SendThread = (HANDLE)_beginthreadex(NULL, 0, NetworkServer::SendThread, this, 0, NULL);
//
//	return true;
//}
//
//bool NetworkServer::DisconnectAllUser()
//{
//	for (size_t i = 0; i < m_MaxUserCount; ++i)
//	{
//		if (m_SessionArray[i]->_USED)
//		{
//			ReleaseSocket(m_SessionArray[i]);
//		}
//	}
//	bool bAllUserRelease = true;
//	while (true)
//	{
//		bAllUserRelease = true;
//		for (size_t i = 0; i < m_MaxUserCount; ++i)
//		{
//			if (m_SessionArray[i]->_USED)
//			{
//				bAllUserRelease = false;
//			}
//		}
//		if (bAllUserRelease)
//		{
//			for (size_t i = 0; i < m_MaxUserCount; ++i)
//			{
//				delete m_SessionArray[i];
//			}
//			delete[] m_SessionArray;
//
//			return true;
//		}
//	}
//
//	return bAllUserRelease;
//}
//
//void NetworkServer::ReleaseSocket(NetworkSession* session)
//{
//
//	if (0 == InterlockedExchange(&session->_CloseFlag, 1))
//	{
//		//------------------------------------------------------
//		// socket을 지우기전에, 먼저 session에있는 소켓부터 InvalidSocket으로 치환한다.
//		//------------------------------------------------------
//
//		SOCKET temp = session->_Socket;
//
//		session->_Socket = INVALID_SOCKET;
//
//		closesocket(temp);
//
//	}
//
//}
//
//void NetworkServer::SpecialErrorCodeCheck(int32_t errorCode)
//{
//	if (errorCode != WSAECONNRESET && errorCode != WSAECONNABORTED && errorCode != WSAENOTSOCK && errorCode != WSAEINTR)
//	{
//		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Special ErrorCode : %d", errorCode);
//		NetworkServer::Crash();
//	}
//}
//
//
//uint64_t NetworkServer::GetSessionID(uint64_t index)
//{
//	return (index << 48) | (++m_SessionID);
//}
//uint16_t NetworkServer::GetSessionIndex(uint64_t sessionID)
//{
//	uint16_t tempIndex = (uint16_t)((0xffff000000000000 & sessionID) >> 48);
//
//	if (tempIndex <0 || tempIndex >m_MaxUserCount - 1)
//	{
//		Crash();
//	}
//	return tempIndex;
//}
//
//
//void NetworkServer::ReleaseSession(NetworkSession* delSession)
//{
//	if (delSession == nullptr)
//	{
//		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"delte Session이 널이다");
//		return;
//	}
//
//	if (delSession->_ErrorCode == ERROR_SEM_TIMEOUT)
//	{
//		/*InterlockedIncrement(&g_TCPTimeoutReleaseCnt);*/
//	}
//
//	//---------------------------------------
//	// CAS로 IOCOUNT의 최상위비트를  ReleaseFlag로 쓴다
//	//---------------------------------------
//
//	ReleaseSocket(delSession);
//
//
//	if (delSession->_IOCount > 0)
//	{
//		NetworkServer::Crash();
//	}
//
//	ReleasePacket(delSession);
//	if (delSession->_SendQ.GetQCount() > 0)
//	{
//		NetworkServer::Crash();
//	}
//
//
//	if (delSession->_DeQPacketArray[0] != nullptr)
//	{
//		NetworkServer::Crash();
//	}
//
//	SessionClear(delSession);
//
//	delSession->OnClientLeave_Auth();
//	InterlockedDecrement(&m_SessionCount);
//}
//
//void NetworkServer::SessionClear(NetworkSession* session)
//{
//#if MEMORYLOG_USE ==1 ||   MEMORYLOG_USE ==2
//	IOCP_Log log;
//#endif
//
//	session->_Socket = INVALID_SOCKET;
//	int64_t tempID = session->_ID;
//	session->_CloseFlag = 0;
//	//-------------------------------------------------
//	// Release Flag 초기화 및 Accept 시 WSARecv에 걸 IOCount 미리 증가시킴
//	//-------------------------------------------------
//	InterlockedIncrement(&session->_IOCount);
//
//	session->_SendFlag = 0;
//	if (session->_SendQ.GetQCount() > 0)
//	{
//		NetworkServer::Crash();
//	}
//
//	session->_RecvRingQ.ClearBuffer();
//
//	//-----------------------------------------------------------------------
//	// SendPacket전에,Session안에있는 CompleteQ가 보내기전 종료된다면
//	// 메모리가 누수날수있고 잘못된 메시지를 보낼수있기때문에 이를 처리해줘야함.
//	//-----------------------------------------------------------------------
//	NetPacket* deqPacket = nullptr;
//
//	while (session->_CompleteRecvPacketQ.Dequeue(&deqPacket))
//	{
//		deqPacket->Free(deqPacket);
//	}
//	session->_CompleteRecvPacketQ.ClearBuffer();
//
//	ZeroMemory(&session->_RecvOL, sizeof(session->_RecvOL));
//	ZeroMemory(&session->_SendOL, sizeof(session->_SendOL));
//
//
//	ZeroMemory(session->_IP, sizeof(session->_IP));
//	session->_Port = 0;
//
//	//-----------------------------------------
//	// For Debug
//	//-----------------------------------------
//	//session->_MemoryLog_IOCP.Clear();
//	//session->_MemoryLog_SendFlag.Clear();
//	session->_SendByte = 0;
//	session->_USED = false;
//	session->_ErrorCode = 0;
//	session->_GQCSRtn = TRUE;
//	session->_TransferZero = 0;
//	session->_IOFail = false;
//
//	session->_bIOCancel = false;
//	session->_bReserveDiscon = false;
//	//-----------------------------------------------
//	// Session Index 반환
//	//-----------------------------------------------
//	session->_bReleaseReady = false;
//	session->_SessionStatus = eSessionStatus::NOT_USED;
//	m_IndexStack->Push(GetSessionIndex(tempID));
//}
//
//void NetworkServer::SetTimeOut(NetworkSession* session)
//{
//	session->_TimeOut = m_TimeOutOption._HeartBeatTimeOut;
//}
//
//void NetworkServer::SetTimeOut(NetworkSession* session, DWORD timeOut)
//{
//	session->_TimeOut = timeOut;
//}
//
//void NetworkServer::SendNDiscon(NetworkSession* session, NetPacket* packet)
//{
//	//---------------------------------------
//   // 보낸 뒤, TimeOut 시간을 2초로 조정한다
//   // Client에서 주는 프로토콜이 없다면 끊길것이다.
//   //---------------------------------------
//	//session->SendUnicast(packet);
//	SetTimeOut(session, 2000);
//}
//
//
//
//void NetworkServer::DeQPacket(NetworkSession* session)
//{
//	for (int i = 0; i < session->_DeQArraySize; ++i)
//	{
//		NetPacket* delNetPacket = session->_DeQPacketArray[i];
//		session->_DeQPacketArray[i] = nullptr;
//
//		if (0 == delNetPacket->DecrementRefCount())
//		{
//			delNetPacket->Free(delNetPacket);
//		}
//
//		//----------------------------------------------
//		// Send 완료통지 후 처리되는것을 TPS로 카운팅 한다.
//		//----------------------------------------------
//		InterlockedIncrement(&m_SendTPS);
//	}
//	session->_DeQArraySize = 0;
//}
//
//void NetworkServer::ReleasePacket(NetworkSession* session)
//{
//	NetPacket* delNetPacket = nullptr;
//
//	while (session->_SendQ.DeQ(&delNetPacket))
//	{
//		if (0 == delNetPacket->DecrementRefCount())
//		{
//			delNetPacket->Free(delNetPacket);
//		}
//	}
//
//	//-----------------------------------------------------
//	// SendPacket 후, SendPost까지했는데
//	// 상대방이 연결을 끊은 경우 Send 완료통지가 오지않은경우 남아있을 수 있다
//	// 이를 위해 처리를 해줘야된다.
//	//-----------------------------------------------------
//	if (session->_DeQArraySize > 0)
//	{
//		DeQPacket(session);
//	}
//}


//
//void NetworkServer::AcceptUser(SOCKET socket, WCHAR* ip, uint16_t port)
//{
//	ConnectInfo* conInfo = m_ConInfoPool.Alloc();
//
//	wcscpy_s(conInfo->_IP, ip);
//	conInfo->_Port = port;
//	conInfo->_Socket = socket;
//
//	while (!m_SocketQ.Enqueue(conInfo))
//	{
//
//	}
//}
//void NetworkServer::CreateNewSession(ConnectInfo* conInfo)
//{
//	NetworkSession* newSession = nullptr;
//	uint64_t index = 0;
//
//	while (!m_IndexStack->Pop(&index))
//	{
//		wprintf(L"Pop Indexing\n");
//	}
//
//	newSession = m_SessionArray[index];
//
//	if (newSession->_SessionStatus != eSessionStatus::NOT_USED)
//	{
//		Crash();
//	}
//	if (newSession == nullptr)
//	{
//		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Session이 모두 사용중입니다");
//		NetworkServer::Crash();
//	}
//	//-----------------------------------------------------------------
//	// Accept한 유저의 기본 IOCount는 1이다
//	//-----------------------------------------------------------------
//
//	newSession->_Index = index;
//	newSession->_Socket = conInfo->_Socket;
//
//
//	//-----------------------------------------------------------------
//	// SessionID를 갱신 한후, 그때 RelaseFlag를 초기화해준다.
//	//-----------------------------------------------------------------
//	newSession->_ID = GetSessionID(index);
//
//	wcscpy_s(newSession->_IP, conInfo->_IP);
//	newSession->_Port = conInfo->_Port;
//	newSession->_USED = true;
//	//--------------------------------------------
//	if (m_TimeOutOption._OptionOn)
//	{
//		newSession->_TimeOut = m_TimeOutOption._LoginTimeOut;
//	}
//	newSession->_LastRecvTime = timeGetTime();
//
//	//--------------------------------------------
//
//	//------------------------------------------
//	// For Debug
//	//------------------------------------------
//	uint64_t tempOrderIndex = (newSession->_OrderIndex++) % 3;
//	newSession->_LastSessionID[tempOrderIndex][0] = newSession->_SessionOrder++;
//	newSession->_LastSessionID[tempOrderIndex][1] = newSession->_ID;
//	newSession->_LastSessionID[tempOrderIndex][2] = newSession->_Socket;
//
//
//	m_ConInfoPool.Free(conInfo);
//
//	DWORD tempTransfer;
//	DWORD tempFlag = 0;
//	BOOL bResult = WSAGetOverlappedResult(newSession->_Socket, &newSession->_RecvOL, &tempTransfer, FALSE, &tempFlag);
//	if (!bResult)
//	{
//		int error = WSAGetLastError();
//		if (error == WSA_IO_INCOMPLETE)
//		{
//			NetworkServer::Crash();
//		}
//	}
//
//	if (m_SessionID > UINT64_MAX - 1)
//	{
//		NetworkServer::Crash();
//	}
//
//	if (NULL == CreateIoCompletionPort((HANDLE)newSession->_Socket, m_IOCP, (ULONG_PTR)newSession, 0))
//	{
//		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"소켓과 IOCP 연결실패:%d", GetLastError());
//		newSession->_USED = false;
//		return;
//	}
//
//	//_LOG(LOG_LEVEL_DEBUG, L"Sesssion[%d]이 연결되었습니다", newSession->_ID);
//
//	InterlockedIncrement(&m_SessionCount);//m_SessionCount
//
//	newSession->OnClientJoin_Auth(newSession->_IP, newSession->_Port);
//
//
//	WSABUF tempBuffer[1];
//	DWORD flag = 0;
//
//	tempBuffer[0].buf = newSession->_RecvRingQ.GetRearBufferPtr();
//	tempBuffer[0].len = newSession->_RecvRingQ.GetDirectEnqueueSize();
//
//	if (tempBuffer[0].len <= 0)
//	{
//		NetworkServer::Crash();
//	}
//
//	int recvRtn = WSARecv(newSession->_Socket, tempBuffer, 1, NULL, &flag, &newSession->_RecvOL, NULL);
//
//	newSession->_SessionStatus = eSessionStatus::AUTH;
//
//	if (recvRtn == SOCKET_ERROR)
//	{
//		int errorCode = WSAGetLastError();
//		if (errorCode != WSA_IO_PENDING)
//		{
//			newSession->_IOFail = true;
//
//			DWORD tempIOCount = InterlockedDecrement(&newSession->_IOCount);
//
//			SpecialErrorCodeCheck(errorCode);
//			if (0 == tempIOCount)
//			{
//				//MMOGameLib::Crash();
//				newSession->_bReleaseReady = true;
//			}
//		}
//	}
//}
//bool NetworkServer::RecvPacket(NetworkSession* curSession, DWORD transferByte)
//{
//	//-----------------------------------------
//	// Enqueue 확정
//	//-----------------------------------------
//	if ((int)transferByte > curSession->_RecvRingQ.GetFreeSize())
//	{
//		//curSession->Disconnect();
//
//		return false;
//	}
//	curSession->_RecvRingQ.MoveRear(transferByte);
//
//	curSession->_LastRecvTime = timeGetTime();
//
//
//	InterlockedAdd(&m_NetworkRecvTraffic, transferByte + 40);
//
//	while (true)
//	{
//		NetHeader header;
//		NetPacket* packet;
//
//		int usedSize = curSession->_RecvRingQ.GetUsedSize();
//
//		if (usedSize < sizeof(NetHeader))
//		{
//			break;
//		}
//		int peekRtn = curSession->_RecvRingQ.Peek((char*)&header, sizeof(NetHeader));
//
//		if (header._Code != dfPACKET_CODE)
//		{
//			//----------------------------------------
//			// 헤더의 코드가 다를 경우 유저를 끊는다.
//			//----------------------------------------
//			_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"헤더 코드가 다름 [Session ID:%llu] [Code:%d]", curSession->_ID, header._Code);
//			_LOG->WriteLogHex(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Recv RingQ Hex", (BYTE*)curSession->_RecvRingQ.GetFrontBufferPtr(), curSession->_RecvRingQ.GetDirectDequeueSize());
//
//			//curSession->Disconnect();
//			return false;
//		}
//		if (header._Len <= 0)
//		{
//			//----------------------------------------
//			// 헤더안에 표기된 Len이 0보다 같거나 작으면 역시 끊는다.
//			//----------------------------------------
//
//			_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"헤더의 Len :0  [Session ID:%llu] [Code:%d]", curSession->_ID, header._Len);
//			//curSession->Disconnect();
//
//			return false;
//		}
//		if (usedSize - sizeof(NetHeader) < header._Len)
//		{
//			//-------------------------------------
//			// 들어오려고하는패킷이, 내 링버퍼 현재 여유사이즈보다 크면 말이안되기때문에,
//			// 그런 Session은 연결을 끊는다.
//			//-------------------------------------
//			if (header._Len > curSession->_RecvRingQ.GetFreeSize())
//			{
//				//curSession->Disconnect();
//				return false;
//			}
//			break;
//		}
//		curSession->_RecvRingQ.MoveFront(sizeof(header));
//
//		//packet = NetPacket::Alloc();
//
//		int deQRtn = curSession->_RecvRingQ.Dequeue((*packet).GetPayloadPtr(), header._Len);
//
//
//		if (deQRtn != header._Len)
//		{
//			NetworkServer::Crash();
//		}
//		(*packet).MoveWritePos(deQRtn);
//
//		if ((*packet).GetPayloadSize() == 0)
//		{
//			NetworkServer::Crash();
//		}
//
//		if (!packet->Decoding(&header))
//		{
//			//-----------------------------------
//			// Decoding 실패시, 유저를 끊는다.
//			//-----------------------------------
//			packet->Free(packet);
//			//curSession->Disconnect();
//			return false;
//		}
//		InterlockedIncrement((LONG*)&m_RecvTPS);
//
//		while (!curSession->_CompleteRecvPacketQ.Enqueue(packet))
//		{
//			
//		}
//
//		//curSession->OnRecv(packet);
//
//	}
//
//	RecvPost(curSession);
//
//	return true;
//}
//bool NetworkServer::RecvPost(NetworkSession* curSession)
//{
//	//SendFlag_Log sendFlagLog;
//	//-------------------------------------------------------------
//	// Recv 걸기
//	//-------------------------------------------------------------
//	if (curSession->_RecvRingQ.GetFreeSize() <= 0)
//	{
//		//curSession->Disconnect();
//		return false;
//	}
//	if (curSession->_IOCount <= 0)
//	{
//		NetworkServer::Crash();
//	}
//
//	DirectData directData;
//	int bufCount = 0;
//
//	curSession->_RecvRingQ.GetDirectEnQData(&directData);
//
//	WSABUF wsaRecvBuf[2];
//
//	wsaRecvBuf[0].buf = directData.bufferPtr1;
//	wsaRecvBuf[0].len = directData._Direct1;
//	bufCount = 1;
//
//	if (directData._Direct2 != 0)
//	{
//		bufCount = 2;
//		wsaRecvBuf[1].buf = directData.bufferPtr2;
//		wsaRecvBuf[1].len = directData._Direct2;
//	}
//
//	if (directData._Direct1 <0 || directData._Direct2<0 || directData._Direct1> RingQ::RING_BUFFER_SIZE || directData._Direct2> RingQ::RING_BUFFER_SIZE)
//	{
//		NetworkServer::Crash();
//	}
//	DWORD flag = 0;
//
//	if (curSession->_IOCount <= 0)
//	{
//		NetworkServer::Crash();
//	}
//
//	InterlockedIncrement(&curSession->_IOCount);
//
//	ZeroMemory(&curSession->_RecvOL, sizeof(curSession->_RecvOL));
//
//	if (curSession->_IOCount <= 0)
//	{
//
//		NetworkServer::Crash();
//	}
//
//	//------------------------------------------------------------------
//	// 	   IO Cancel 이 실행됬다면, 입출력을 걸지않고, IOCount를 낮추고 Return한다
//	// 
//	//------------------------------------------------------------------
//	if (curSession->_bIOCancel)
//	{
//		if (0 == InterlockedDecrement(&curSession->_IOCount))
//		{
//			curSession->_bReleaseReady = true;
//		}
//
//		return false;
//	}
//	int recvRtn = WSARecv(curSession->_Socket, wsaRecvBuf, bufCount, NULL, &flag, &curSession->_RecvOL, NULL);
//	if (curSession->_IOCount <= 0)
//	{
//		NetworkServer::Crash();
//	}
//
//	if (recvRtn == SOCKET_ERROR)
//	{
//		int errorCode = WSAGetLastError();
//		if (errorCode != WSA_IO_PENDING)
//		{
//			curSession->_IOFail = true;
//			int tempIOCount = InterlockedDecrement(&curSession->_IOCount);
//
//			SpecialErrorCodeCheck(errorCode);
//			if (0 == tempIOCount)
//			{
//				NetworkServer::Crash();
//				curSession->_bReleaseReady = true;
//			}
//			if (tempIOCount < 0)
//			{
//				NetworkServer::Crash();
//			}
//			return false;
//		}
//	}
//	return true;
//}





int64_t NetworkServer::GetAcceptCount()
{
	return m_AcceptCount;
}

LONG NetworkServer::GetAcceptTPS()
{
	return m_AcceptTPS_To_Main;
}

LONG NetworkServer::GetSendTPS()
{
	return m_SendTPS_To_Main;
}

LONG NetworkServer::GetRecvTPS()
{
	return m_RecvTPS_To_Main;
}

LONG NetworkServer::GetNetworkTraffic()
{
	return m_NetworkTraffic_To_Main;
}

LONG NetworkServer::GetNetworkRecvTraffic()
{
	return m_NetworkRecvTraffic_To_Main;
}

LONG NetworkServer::GetSessionCount()
{
	return m_SessionCount;
}


LONG NetworkServer::GetSendQMeomryCount()
{
	return m_SendQMemory;
}

LONG NetworkServer::GetLockFreeStackMemoryCount()
{
	if (m_IndexStack == nullptr)
	{
		return 0;
	}
	return m_IndexStack->GetMemoryAllocCount();
}

DWORD NetworkServer::GetAuthFPS()
{
	return m_AuthFPS_To_Main;
}

DWORD NetworkServer::GetGameFPS()
{
	return m_GameFPS_To_Main;
}

DWORD NetworkServer::GetSendFPS()
{
	return m_SendFPS_To_Main;
}


//void NetworkServer::SetSessionArray(NetworkSession* sessionArray,DWORD maxUserCount)
//{
//	for (DWORD i = 0; i < maxUserCount; ++i)
//	{
//		m_SessionArray[i] = sessionArray+i;
//	}
//}
////unsigned int __stdcall NetworkServer::AcceptThread(LPVOID param)
////{
////	NetworkServer* mmoServer = (NetworkServer*)param;
////
////	while (true)
////	{
////		SOCKADDR_IN clientAddr;
////		ZeroMemory(&clientAddr, sizeof(clientAddr));
////		int addrLen = sizeof(clientAddr);
////		SOCKET clientSocket;
////
////		clientSocket = accept(mmoServer->m_ListenSocket, (sockaddr*)&clientAddr, &addrLen);
////		if (clientSocket == INVALID_SOCKET)
////		{
////			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"accept () error:%d", WSAGetLastError());
////			break;
////		}
////		mmoServer->m_AcceptCount++;
////
////		WCHAR tempIP[17] = { 0, };
////		uint16_t tempPort = ntohs(clientAddr.sin_port);
////		InetNtop(AF_INET, &clientAddr.sin_addr.S_un.S_addr, tempIP, 17);
////
////		////----------------------------------------------------//
////		////Black IP 차단 및 White IP 세션생성 
////		////----------------------------------------------------//
////		//if (!mmoServer->m_Contents->OnConnectionRequest(tempIP, tempPort))
////		//{
////		//	closesocket(clientSocket);
////		//	continue;
////		//}
////		mmoServer->AcceptUser(clientSocket, tempIP, tempPort);
////		mmoServer->m_AcceptTPS++;
////
////	}
////
////	_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"Accpt Thread[%d] 종료", GetCurrentThreadId());
////	return 0;
////}

//unsigned int __stdcall NetworkServer::WorkerThread(LPVOID param)
//{	
//	NetworkServer* mmoServer = (NetworkServer*)param;
//
//	while (true)
//	{
//		DWORD transferByte = 0;
//		OVERLAPPED* curOverlap = nullptr;
//		NetworkSession* curSession = nullptr;
//		BOOL gqcsRtn = GetQueuedCompletionStatus(mmoServer->m_IOCP, &transferByte, (PULONG_PTR)&curSession, (LPOVERLAPPED*)&curOverlap, INFINITE);
//
//		int errorCode;
//
//		if (gqcsRtn == FALSE)
//		{
//			errorCode = WSAGetLastError();
//			curSession->_ErrorCode = errorCode;
//		}
//
//		//----------------------------------------------
//		// GQCS에서 나온 오버랩이 nullptr이면, 타임아웃, IOCP자체가 에러 , 아니면 postQ로 NULL을 넣었을때이다
//		//----------------------------------------------
//		if (curOverlap == NULL && curSession == NULL && transferByte == 0)
//		{
//			break;
//		}
//		do
//		{
//			if (curSession == nullptr)
//			{
//				mmoServer->NetworkServer::Crash();
//			}
//			if (curSession->_IOCount == 0)
//			{
//				mmoServer->NetworkServer::Crash();
//			}
//			curSession->_GQCSRtn = gqcsRtn;
//
//			uint64_t sessionID = curSession->_ID;
//
//			if (transferByte == 0)
//			{
//				if (curOverlap == &curSession->_RecvOL)
//				{
//
//					if (gqcsRtn == TRUE)
//					{
//						mmoServer->NetworkServer::Crash();
//					}
//					curSession->_TransferZero = 5;
//				}
//				else if (curOverlap == &curSession->_SendOL)
//				{
//					if (gqcsRtn == TRUE)
//					{
//						mmoServer->NetworkServer::Crash();
//					}
//					curSession->_TransferZero = 6;
//				}
//				//----------------------------------------------
//				// 작업 실패시 close socket을 해준다
//				//----------------------------------------------
//				break;
//			}
//
//			if (curOverlap == &curSession->_RecvOL)
//			{
//				if (curSession->_IOCount <= 0)
//				{
//					mmoServer->NetworkServer::Crash();
//				}
//				mmoServer->RecvPacket(curSession, transferByte);
//			}
//			else if (curOverlap == &curSession->_SendOL)
//			{
//				if (curSession->_IOCount <= 0)
//				{
//					mmoServer->NetworkServer::Crash();
//				}
//
//				if (curSession->_SendByte != transferByte)
//				{
//					mmoServer->NetworkServer::Crash();
//				}
//
//				mmoServer->DeQPacket(curSession);
//				curSession->_SendByte = 0;
//
//				////-------------------------------------------------------
//				//// send 완료통지가 왔기때문에 SendFlag를 바꿔준다. 
//				////-------------------------------------------------------
//				InterlockedExchange(&curSession->_SendFlag, 0);
//
//				InterlockedAdd(&mmoServer->m_NetworkTraffic, transferByte + 40);
//			}
//			else
//			{
//				// 오버랩 들어온거비교
//				//--------------------------------
//				// For Devbug
//				//--------------------------------
//				mmoServer->NetworkServer::Crash();
//			}
//
//		} while (0);
//
//
//		if (curSession->_IOCount <= 0)
//		{
//			mmoServer->NetworkServer::Crash();
//		}
//
//		//-------------------------------------------------------------
//		// 완료통지로 인한 IO 차감
//		//-------------------------------------------------------------
//		int tempIOCount = InterlockedDecrement(&curSession->_IOCount);
//
//		if (0 == tempIOCount)
//		{
//			curSession->_bReleaseReady = true;
//		}
//
//	}
//	_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"WorkerThread[%d] 종료", GetCurrentThreadId());
//
//	return 0;
//}

//unsigned int __stdcall NetworkServer::MonitorThread(LPVOID param)
//{
//
//	NetworkServer* mmoServer = (NetworkServer*)param;
//	DWORD maxUserCount = mmoServer->m_MaxUserCount;
//	NetworkSession** sessionArray = mmoServer->m_SessionArray;
//
//
//	while (true)
//	{
//		DWORD rtn = WaitForSingleObject(mmoServer->m_MonitorEvent, 999);
//		if (rtn != WAIT_TIMEOUT)
//		{
//			break;
//		}
//
//		time_t dataTime;
//
//		time(&dataTime);
//
//		//mmoServer->m_MonitorClient->SendPacket(m_MonitorClient)
//
//		//-------------------------------------------------------------
//		// 모든 세션의 락프리큐 노드 합산
//		//-------------------------------------------------------------
//		mmoServer->m_SendQMemory = 0;
//
//		for (DWORD i = 0; i < mmoServer->m_MaxUserCount; ++i)
//		{
//			NetworkSession* curSession = mmoServer->m_SessionArray[i];
//
//			mmoServer->m_SendQMemory += curSession->_SendQ.GetMemoryPoolAllocCount();
//		}
//
//
//		mmoServer->m_RecvTPS_To_Main = mmoServer->m_RecvTPS;
//		mmoServer->m_SendTPS_To_Main = mmoServer->m_SendTPS;
//		mmoServer->m_AcceptTPS_To_Main = mmoServer->m_AcceptTPS;
//		mmoServer->m_NetworkTraffic_To_Main = mmoServer->m_NetworkTraffic;
//		mmoServer->m_NetworkRecvTraffic_To_Main = mmoServer->m_NetworkRecvTraffic;
//
//
//		InterlockedExchange(&mmoServer->m_RecvTPS, 0);
//		InterlockedExchange(&mmoServer->m_SendTPS, 0);
//		InterlockedExchange(&mmoServer->m_AcceptTPS, 0);
//		InterlockedExchange(&mmoServer->m_NetworkTraffic, 0);
//		InterlockedExchange(&mmoServer->m_NetworkRecvTraffic, 0);
//	}
//
//	_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"MonitorThread 종료");
//
//	return 0;
//}

//unsigned int __stdcall NetworkServer::GameLogicThread(LPVOID param)
//{
//	NetworkServer* mmoServer = (NetworkServer*)param;
//	DWORD maxUserCount = mmoServer->m_MaxUserCount;
//	NetworkSession** sessionArray = mmoServer->m_SessionArray;
//	int fpsCount = 0;
//
//	DWORD time = timeGetTime();
//	while (true)
//	{
//		DWORD  rtnWait = WaitForSingleObject(mmoServer->m_GameThreadEvent, 5);
//
//		if (rtnWait == WAIT_FAILED)
//		{
//			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Wait Fail GameLogicThread:%d", GetCurrentThreadId());
//			mmoServer->NetworkServer::Crash();
//		}
//		//-----------------------------------------------------
//		// 종료 신호 왔을시,  Game Logic  스레드종료
//		//-----------------------------------------------------
//		if (rtnWait != WAIT_TIMEOUT)
//		{
//			break;
//		}
//
//		fpsCount++;
//
//		if (timeGetTime() - time > 1000)
//		{
//			mmoServer->m_GameFPS_To_Main = fpsCount;
//			fpsCount = 0;
//			time = timeGetTime();
//		}
//
//		for (DWORD i = 0; i < maxUserCount; ++i)
//		{
//			NetworkSession* curSession = sessionArray[i];
//
//			if (curSession->_SessionStatus == eSessionStatus::AUTH_TO_GAME)
//			{
//				//----------------------------------------------
//				// Auth 에서 Game으로 넘어온 애들을 초기화 (섹터할당,..기타등등)을끝내고 Game상태로 바꾼다
//				//----------------------------------------------
//				curSession->_SessionStatus = eSessionStatus::GAME;
//
//				mmoServer->m_GameSessionCount++;
//
//			}
//
//			if (curSession->_SessionStatus == eSessionStatus::GAME)
//			{
//				if (curSession->_bReleaseReady)
//				{
//					curSession->_SessionStatus = eSessionStatus::GAME_RELEASE;
//				}
//				else
//				{
//					
//					//wprintf(L"QCount:%d", curSession->_CompleteRecvPacketQ.GetUsedSize());
//					while (true)
//					{
//						NetPacket* packet = nullptr;
//					
//						if (curSession->_CompleteRecvPacketQ.Dequeue(&packet) == true)
//						{
//							curSession->OnGamePacket(packet);
//						}
//						else
//						{
//							break;
//						}
//					}
//					
//				}
//			}
//			
//			if (curSession->_SessionStatus == eSessionStatus::GAME_RELEASE && curSession->_bSending == false)
//			{
//				curSession->_SessionStatus = eSessionStatus::RELEASE;
//			}
//
//			if (curSession->_SessionStatus == eSessionStatus::RELEASE)
//			{
//				//-----------------------------------------------------
//				// Game스레드에서만 Release를 진행한다.
//				// ReleaseSession 내부에서 상태를 Release -> Not Used 로 바꿈
//				//-----------------------------------------------------
//				mmoServer->ReleaseSession(curSession);
//				mmoServer->m_GameSessionCount--;
//
//			}
//
//		}
//
//
//	}
//	return 0;
//}

//unsigned int __stdcall NetworkServer::AuthThread(LPVOID param)
//{
//	NetworkServer* mmoServer = (NetworkServer*)param;
//	DWORD maxUserCount = mmoServer->m_MaxUserCount;
//	NetworkSession** sessionArray = mmoServer->m_SessionArray;
//	int fpsCount = 0;
//
//	DWORD time = timeGetTime();
//
//	while (true)
//	{
//		DWORD  rtnWait = WaitForSingleObject(mmoServer->m_AuthTreadEvent, 10);
//
//
//		if (timeGetTime() - time > 1000)
//		{
//			mmoServer->m_AuthFPS_To_Main = fpsCount;
//			fpsCount = 0;
//			time = timeGetTime();
//		}
//
//		if (rtnWait == WAIT_FAILED)
//		{
//			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Wait Fail AuthThread:%d", GetCurrentThreadId());
//			mmoServer->NetworkServer::Crash();
//		}
//		//-----------------------------------------------------
//		// 종료 신호 왔을시,  AuthThread 스레드종료
//		//-----------------------------------------------------
//		if (rtnWait  != WAIT_TIMEOUT)
//		{
//			break;
//		}
//
//		++fpsCount;
//
//		ConnectInfo* conInfo;
//
//
//		//-------------------------------------------------------------
//		// 미인증상태 ->  Auth상태
//		//-------------------------------------------------------------
//
//		while (mmoServer->m_SocketQ.Dequeue(&conInfo))
//		{
//			//-----------------------------------------------------
//			// ReleaseSession 내부에서 상태를 NotUsed->Auth 상태로바꿈.
//			//-----------------------------------------------------
//			mmoServer->CreateNewSession(conInfo);
//			++mmoServer->m_AuthSessionCount;
//
//		}
//
//		//-------------------------------------------------------------
//		// Auth상태 -> 패킷처리, Release 처리
//		//-------------------------------------------------------------
//		for (DWORD i = 0; i < maxUserCount; ++i)
//		{
//			NetworkSession* curSession = sessionArray[i];
//
//			if (curSession->_SessionStatus == eSessionStatus::AUTH)
//			{	
//				if (curSession->_bReleaseReady)
//				{
//					curSession->_SessionStatus = eSessionStatus::AUTH_RELEASE;
//				}
//				else
//				{
//					NetPacket* packet;
//
//					if (curSession->_CompleteRecvPacketQ.Dequeue(&packet))
//					{
//						curSession->OnAuthPacket(packet);
//						//-------------------------------------------------
//						// OnAuthPacket으로 로그인패킷이 올것이므로, 여기서 DB처리후,
//						// Auth_To_Game상태로 바꾼다.
//						//-------------------------------------------------
//						curSession->_SessionStatus = eSessionStatus::AUTH_TO_GAME;
//						--mmoServer->m_AuthSessionCount;
//
//					}
//				}
//			}
//
//			if (curSession->_SessionStatus == eSessionStatus::AUTH_RELEASE && curSession->_bSending == false)
//			{
//				curSession->_SessionStatus = eSessionStatus::RELEASE;
//				--mmoServer->m_AuthSessionCount;
//			}
//			
//		}
//	}
//	return 0;
//}

//unsigned int __stdcall NetworkServer::SendThread(LPVOID param)
//{
//	NetworkServer* mmoServer = (NetworkServer*)param;
//	DWORD maxUserCount = mmoServer->m_MaxUserCount;
//	NetworkSession** sessionArray = mmoServer->m_SessionArray;
//	int fpsCount = 0;
//	DWORD time = timeGetTime();
//
//	while (true)
//	{
//		DWORD  rtnWait = WaitForSingleObject(mmoServer->m_SendThreadEvent, 4);
//
//		if (rtnWait == WAIT_FAILED)
//		{
//			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Wait Fail SendThread:%d", GetCurrentThreadId());
//			mmoServer->NetworkServer::Crash();
//		}
//		//-----------------------------------------------------
//		// 종료 신호 왔을시,  SendThread 스레드종료
//		//-----------------------------------------------------
//		if (rtnWait != WAIT_TIMEOUT)
//		{
//			break;
//		}
//		++fpsCount;
//
//		if (timeGetTime() - time > 1000)
//		{
//			mmoServer->m_SendFPS_To_Main = fpsCount;
//			fpsCount = 0;
//			time = timeGetTime();
//		}
//
//		for (DWORD i = 0; i < maxUserCount; ++i)
//		{
//			NetworkSession* curSession = sessionArray[i];
//			//uint64_t  tempID = curSession->_ID;
//
//			curSession->_bSending = true;
//
//			if (curSession->_SessionStatus == eSessionStatus::AUTH || curSession->_SessionStatus == eSessionStatus::GAME)
//			{				
//				//curSession->SendPost();
//			}
//			else 
//			{
//				curSession->_bSending = false;
//			}
//
//		}
//	}
//	return 0;
//}

void NetworkServer::Crash()
{
	int* p = nullptr;
	*p = 10;
}






