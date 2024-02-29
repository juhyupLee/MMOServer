#include "LanClientLib.h"
#include "Log.h"


LanClient::LanClient(LanClient* contents)
	:m_Contents(contents)
{
	setlocale(LC_ALL, "");

	m_WorkerThread =nullptr;
	m_MonitoringThread = INVALID_HANDLE_VALUE;

	m_ServerPort = 0;
	m_ServerIP =nullptr;

	m_WorkerThreadCount =0;

	m_IOCP = INVALID_HANDLE_VALUE;
	
	m_RecvTPS =0;
	m_SendTPS =0;

	m_WorkerThread = nullptr;
	m_MonitorEvent = INVALID_HANDLE_VALUE;

	m_SendFlagNo = -1;


	m_RecvTPS_To_Main = 0;
	m_SendTPS_To_Main=0;
	m_SendQMemory = 0;


	m_NetworkTraffic = 0;
	m_NetworkTraffic_To_Main = 0;
	m_Session = nullptr;

}

LanClient::~LanClient()
{
	if (!m_Session)
	{
		delete m_Session;
	}
	if (!m_WorkerThread)
	{
		delete[] m_WorkerThread;
	}
}

bool LanClient::Connect(const WCHAR* ip, uint16_t port, DWORD runningThread, DWORD workerThreadCount,SocketOption& option)
{
	timeBeginPeriod(1);

	if (!NetworkInit(ip, port, runningThread, option))
	{
		_LOG->WriteLog(L"LanClient", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"NetworkInit Fail");
	}


	EventInit();

	ThreadInit(workerThreadCount);


	if (connect(m_Session->_Socket, (SOCKADDR*)&m_ServerAddr, sizeof(m_ServerAddr)) == SOCKET_ERROR)
	{
		_LOG->WriteLog(L"LanClient", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Connect Fail:%d",WSAGetLastError());
		return false;
	}
	
	_LOG->WriteLog(L"LanClient", SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"LanClient Connect.....");
    return true;
}

void LanClient::ClientStop()
{
	timeEndPeriod(1);

	SetEvent(m_MonitorEvent);
	WaitForSingleObject(m_MonitoringThread, INFINITE);


	////Disconnect()
	//if (DisconnectAllUser())
	//{
	//	for (size_t i = 0; i < m_WorkerThreadCount; ++i)
	//	{
	//		PostQueuedCompletionStatus(m_IOCP, 0, NULL, NULL);
	//		CloseHandle(m_WorkerThread[i]);
	//	}
	//	WaitForMultipleObjects(m_WorkerThreadCount, m_WorkerThread, TRUE, INFINITE);

	//	CloseHandle(m_MonitoringThread);
	//}
	//

	//_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"남은세션:%d", m_SessionCount);
	//_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"서버 종료");



	//delete m_IndexStack;
	//m_IndexStack = nullptr;

	//m_AcceptCount = 0;
	//m_AcceptTPS = 0;
	//m_RecvTPS = 0;
	//m_SendTPS = 0;
	//m_SendQMemory = 0;
	//m_AcceptTPS_To_Main = 0;
	//m_SendTPS_To_Main = 0;
	//m_RecvTPS_To_Main = 0;
	
}



 LONG LanClient::GetSendTPS()
{
	return m_SendTPS_To_Main;
}

 LONG LanClient::GetRecvTPS()
{
	return m_RecvTPS_To_Main;
}

LONG LanClient::GetNetworkTraffic()
{
	return m_NetworkTraffic_To_Main;
}



 int32_t LanClient::GetMemoryAllocCount()
 {
	 return LanPacket::GetMemoryPoolAllocCount();
 }

 LONG LanClient::GetSendQMeomryCount()
 {
	 return m_SendQMemory;
 }


bool LanClient::Disconnect(LanClient::Session* curSession)
{
	if (AcquireSession(curSession))
	{
		/*InterlockedIncrement(&g_DisconnectCount);*/
		if (curSession == nullptr)
		{
			CRASH();
			return false;
		}
		curSession->_bIOCancel = true;
		IO_Cancel(curSession);

		if (0 == InterlockedDecrement(&curSession->_IOCount))
		{
			ReleaseSession(curSession);
		}
	}
    return true;
}




void LanClient::IO_Cancel(Session* curSession)
{
	//-----------------------------------------------
	// Overlapped Pointer가 NULL일시 Send ,Recv 둘다 IO취소한다
	//-----------------------------------------------
	CancelIoEx((HANDLE)curSession->_Socket, NULL);
}

bool LanClient::SendPacket(LanPacket* packet)
{

	if (!AcquireSession(m_Session))
	{
		//---------------------------------------
		// AcqurieSession에 실패했다면 저 패킷에대한 RefCount를 감소시켜야된다.
		//---------------------------------------
		if (packet->DecrementRefCount() == 0)
		{
			/*InterlockedIncrement(&g_FreeMemoryCount);*/
			packet->Free(packet);
		}
		return false;
	}
	


	LanHeader header;
	header._Len = (*packet).GetPayloadSize();
	(*packet).SetHeader(&header);


	if (packet->GetPayloadSize() <= 0)
	{
		CRASH();
	}
	if (!m_Session->_SendQ.EnQ(packet))
	{
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"SendQ 총갯수 초과(LockFreeQ Qcount 초과함)");
		CRASH();
	}

	if (0 == InterlockedDecrement(&m_Session->_IOCount))
	{
		ReleaseSession(m_Session);
	}

    return true;
}


bool LanClient::NetworkInit(const WCHAR* ip, uint16_t port, DWORD runningThread, SocketOption option)
{
	WSAData wsaData;
	
	m_Session = new Session;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"WSAStartUp() Error:%d", WSAGetLastError());
		return false;
	}
	m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, runningThread);

	m_Session->_Socket = socket(AF_INET, SOCK_STREAM, 0);


	if (NULL == CreateIoCompletionPort((HANDLE)m_Session->_Socket, m_IOCP, (ULONG_PTR)m_Session, 0))
	{
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"소켓과 IOCP 연결실패:%d", GetLastError());
		return false;
	}

	if (m_Session->_Socket == INVALID_SOCKET)
	{
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"socket() error:%d", WSAGetLastError());
		return false;
	}

	if (option._SendBufferZero)
	{
		//----------------------------------------------------------------------------
		// 송신버퍼 Zero -->비동기 IO 유도
		//----------------------------------------------------------------------------
		int optVal = 0;
		int optLen = sizeof(optVal);

		int rtnOpt = setsockopt(m_Session->_Socket, SOL_SOCKET, SO_SNDBUF, (const char*)&optVal, optLen);
		if (rtnOpt != 0)
		{
			_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}
	}
	if (option._Linger)
	{
		linger lingerOpt;
		lingerOpt.l_onoff = 1;
		lingerOpt.l_linger = 0;

		int rtnOpt = setsockopt(m_Session->_Socket, SOL_SOCKET, SO_LINGER, (const char*)&lingerOpt, sizeof(lingerOpt));
		if (rtnOpt != 0)
		{
			_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}

	}
	if (option._TCPNoDelay)
	{
		BOOL tcpNodelayOpt = true;

		int rtnOpt = setsockopt(m_Session->_Socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&tcpNodelayOpt, sizeof(tcpNodelayOpt));
		if (rtnOpt != 0)
		{
			_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}
	}

	if (option._KeepAliveOption.onoff)
	{
		DWORD recvByte = 0;

		if (0 != WSAIoctl(m_Session->_Socket, SIO_KEEPALIVE_VALS, &option._KeepAliveOption, sizeof(tcp_keepalive), NULL, 0, &recvByte, NULL, NULL))
		{
			_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}
	}
	ZeroMemory(&m_ServerAddr, sizeof(SOCKADDR_IN));
	m_ServerAddr.sin_family = AF_INET;
	m_ServerAddr.sin_port = htons(port);

	if (ip == nullptr)
	{
		CRASH();
	}
	else
	{
		InetPton(AF_INET, ip, &m_ServerAddr.sin_addr.S_un.S_addr);
	}

	return true;
}

bool LanClient::ThreadInit(DWORD workerThreadCount)
{
	m_WorkerThreadCount = workerThreadCount;
	m_WorkerThread = new HANDLE[workerThreadCount];

	for (size_t i= 0; i < m_WorkerThreadCount; ++i)
	{
		m_WorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, LanClient::WorkerThread, this, 0, NULL);
	}

	m_MonitoringThread = (HANDLE)_beginthreadex(NULL, 0, LanClient::MonitorThread, this, 0, NULL);
	
	return true;
}

bool LanClient::EventInit()
{
	m_MonitorEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	return true;
}



unsigned  __stdcall LanClient::WorkerThread(LPVOID param)
{ 
	LanClient* lanClient = (LanClient*)param;

	while (true)
	{
		DWORD transferByte = 0;
		OVERLAPPED* curOverlap = nullptr;
		Session* curSession = nullptr;
		BOOL gqcsRtn = GetQueuedCompletionStatus(lanClient->m_IOCP, &transferByte, (PULONG_PTR)&curSession, (LPOVERLAPPED*)&curOverlap, INFINITE);

		int errorCode;

		if (gqcsRtn == FALSE)
		{
			errorCode = WSAGetLastError();
			if (errorCode == ERROR_OPERATION_ABORTED)
			{
				//netServer->CRASH();

				//wprintf(L"ErrorCode : %d\n", errorCode);
			}
			curSession->_ErrorCode = errorCode;
		}
		
#if MEMORYLOG_USE ==1 ||   MEMORYLOG_USE ==2
		IOCP_Log log;
#endif

		//SendFlag_Log sendFlagLog;

		//----------------------------------------------
		// GQCS에서 나온 오버랩이 nullptr이면, 타임아웃, IOCP자체가 에러 , 아니면 postQ로 NULL을 넣었을때이다
		//----------------------------------------------
		if (curOverlap == NULL && curSession == NULL && transferByte == 0)
		{
			break;
		}
		//-----------------------------------------------------------------
		// 외부스레드에서 Send할때, PQCS(Transfer=-1, SessionID) 옴
		// 이때 SendPost하고 Continue;
		//-----------------------------------------------------------------
//		if (transferByte == -1 && curSession==NULL)
//		{
//#if MEMORYLOG_USE ==1 
//			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::GQCS, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
//			g_MemoryLog_IOCP.MemoryLogging(log);
//#endif
//
//#if MEMORYLOG_USE  ==2
//			
//			log.DataSettiong(InterlockedIncrement64(&g_IOCPMemoryNo), eIOCP_LINE::PQCS_SENDPOST, GetCurrentThreadId(), curSession->_Socket, curSession->_IOCount, (int64_t)curSession, curSession->_ID, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag);
//			g_MemoryLog_IOCP.MemoryLogging(log);
//			curSession->_MemoryLog_IOCP.MemoryLogging(log);
//#endif
//
//			LanClient->SendPost((uint64_t)curOverlap);
//			continue;
//		}
		do
		{
			
			if (curSession == nullptr)
			{
				CRASH();
			}
			if (curSession->_IOCount == 0)
			{
				CRASH();
			}
			curSession->_GQCSRtn = gqcsRtn;



		/*	sendFlagLog.DataSettiong(InterlockedIncrement64(&netServer->m_SendFlagNo), eSendFlag::GQCS_SENDFLAG, GetCurrentThreadId(), curSession->_Socket, (int64_t)curSession, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag, curSession->_SendQ.GetQCount());
			curSession->_MemoryLog_SendFlag.MemoryLogging(sendFlagLog);
		*/

			if (transferByte == 0)
			{
				if (curOverlap == &curSession->_RecvOL)
				{

					if (gqcsRtn == TRUE)
					{
						CRASH();
					}
					curSession->_TransferZero = 5;
					
					//_LOG(LOG_LEVEL_ERROR, L"TransferByZero Recv  ErrorCode:%d, GQCS Rtn:%d RecvRingQ Size:%d, SendQRingQSize :%d",curSession->_ErrorCode, gqcsRtn, curSession->_RecvRingQ.GetUsedSize(),curSession->_SendQ.m_Count);

				}
				else if (curOverlap == &curSession->_SendOL)
				{
					if (gqcsRtn == TRUE)
					{
						CRASH();
					}
					curSession->_TransferZero = 6;

				}
				//----------------------------------------------
				// 작업 실패시 close socket을 해준다
				//----------------------------------------------
				break;
			}
			
			if (curOverlap == &curSession->_RecvOL)
			{


				if (curSession->_IOCount <= 0)
				{
					CRASH();
				}
				lanClient->RecvPacket(curSession, transferByte);
			}
			else if (curOverlap == &curSession->_SendOL)
			{

				if (curSession->_IOCount <= 0)
				{
					CRASH();
				}

				if (curSession->_SendByte != transferByte)
				{
					CRASH();
				}

				lanClient->DeQPacket(curSession);
				curSession->_SendByte = 0;

				////-------------------------------------------------------
				//// send 완료통지가 왔기때문에 SendFlag를 바꿔준다. 그리고 혹시 그사이에 SendQ에 쌓여있을지모르니 Send Post를 한다
				////-------------------------------------------------------
				InterlockedExchange(&curSession->_SendFlag, 0);
				//g_MemoryLog_SendFlag.MemoryLogging(SEND_COMPLETE_AFTER, GetCurrentThreadId(), curSession->_ID, curSession->_SendFlag, curSession->_SendRingQ.GetUsedSize(),(int64_t)curSession->_Socket, (int64_t)curSession, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL);
				//sendFlagLog.DataSettiong(InterlockedIncrement64(&netServer->m_SendFlagNo), eSendFlag::SEND_COMPLETE_AFTER, GetCurrentThreadId(), curSession->_Socket, (int64_t)curSession, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag, curSession->_SendQ.GetQCount());

				//curSession->_MemoryLog_SendFlag.MemoryLogging(sendFlagLog);
				lanClient->SendPost();

				//netServer->m_NetworkTraffic

				InterlockedAdd(&lanClient->m_NetworkTraffic, transferByte + 40);


			}
			else
			{
				// 오버랩 들어온거비교
				CRASH();
			}

		} while (0);


		if (curSession->_IOCount <= 0)
		{
			CRASH();
		}

		//-------------------------------------------------------------
		// 완료통지로 인한 IO 차감
		//-------------------------------------------------------------

		int tempIOCount = InterlockedDecrement(&curSession->_IOCount);

		if (0 == tempIOCount)
		{
			lanClient->ReleaseSession(curSession);
		}

	}
	_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"WorkerThread[%d] 종료", GetCurrentThreadId());
	
	return 0;
}

unsigned int __stdcall LanClient::MonitorThread(LPVOID param)
{

	LanClient* netServer = (LanClient*)param;

	while (true)
	{
		DWORD rtn = WaitForSingleObject(netServer->m_MonitorEvent, 999);
		if (rtn != WAIT_TIMEOUT)
		{
			break;
		}


		netServer->m_RecvTPS_To_Main = netServer->m_RecvTPS;
		netServer->m_SendTPS_To_Main = netServer->m_SendTPS;
		netServer->m_NetworkTraffic_To_Main = netServer->m_NetworkTraffic;

		InterlockedExchange(&netServer->m_RecvTPS, 0);
		InterlockedExchange(&netServer->m_SendTPS, 0);
		InterlockedExchange(&netServer->m_NetworkTraffic, 0);
		

	}
	
	_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"MonitorThread 종료");
	
	return 0;
}

void LanClient::ReleaseSession(Session * delSession)
{
	if (0 != InterlockedCompareExchange(&delSession->_IOCount, 0x80000000, 0))
	{
		return;
	}

	if (delSession == nullptr)
	{
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"delte Session이 널이다");
		return;
	}
	//------------------------------------------------------------
	// For Debug
	// Client가 끊기전에 패킷을 보내는데, 이게 False라면,
	// Client쪽에서 끊지 않은 Session을 서버가 끊은것임
	//------------------------------------------------------------
	//if (!delSession->_bReserveDiscon)
	//{
	//	CRASH();
	//}
	////-------------------------------------------------------
	////IO Fail true : WSARecv or WSASend Fail
	////transferzero  = 5  (Recv Transfer Zero)
	////-------------------------------------------------------
	//if (delSession->_IOFail == false  &&  delSession->_TransferZero != 5)
	//{
	//	CRASH();
	//}
	//if (delSession->_ErrorCode != ERROR_SEM_TIMEOUT && delSession->_ErrorCode != 64 && delSession->_ErrorCode != 0 && delSession->_ErrorCode !=ERROR_OPERATION_ABORTED)
	//{
	//	CRASH();
	//}

	if (delSession->_ErrorCode == ERROR_SEM_TIMEOUT)
	{
		/*InterlockedIncrement(&g_TCPTimeoutReleaseCnt);*/
	}

	//---------------------------------------
	// CAS로 IOCOUNT의 최상위비트를  ReleaseFlag로 쓴다
	//---------------------------------------
	
	ReleaseSocket(delSession);


	if (delSession->_IOCount < 0x80000000)
	{
		CRASH();
	}

	ReleasePacket(delSession);
	if (delSession->_SendQ.GetQCount() > 0)
	{
		CRASH();
	}


	if (delSession->_DeQPacketArray[0] != nullptr)
	{
		CRASH();
	}
	
	SessionClear(delSession);

	OnLeaveServer();
}

bool LanClient::DisconnectAllUser()
{
	//for (size_t i = 0; i < m_MaxUserCount; ++i)
	//{
	//	if (m_SessionArray[i]._USED)
	//	{
	//		ReleaseSocket(&m_SessionArray[i]);
	//	}
	//}
	//bool bAllUserRelease = true;
	//while (true)
	//{
	//	bAllUserRelease = true;
	//	for (size_t i = 0; i < m_MaxUserCount; ++i)
	//	{
	//		if (m_SessionArray[i]._USED)
	//		{
	//			bAllUserRelease = false;
	//		}
	//	}
	//	if (bAllUserRelease)
	//	{
	//		delete[] m_SessionArray;
	//		return true;
	//	}
	//}

	//return bAllUserRelease;
	return true;
}


bool LanClient::RecvPacket(LanClient::Session* curSession, DWORD transferByte)
{
	//-----------------------------------------
	// Enqueue 확정
	//-----------------------------------------
	if ((int)transferByte > curSession->_RecvRingQ.GetFreeSize())
	{
#if DISCONLOG_USE ==1
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"RecvRingQ 초과사이즈 들어옴 [Session ID:%llu] [transferByte:%d]", curSession->_ID, transferByte);
#endif
		Disconnect(curSession);

		return false;
	}
	curSession->_RecvRingQ.MoveRear(transferByte);

	curSession->_LastRecvTime = timeGetTime();

	
	while (true)
	{
		LanHeader header;
		LanPacket* packet;

		int usedSize = curSession->_RecvRingQ.GetUsedSize();

		if (usedSize < sizeof(LanHeader))
		{
			break;
		}
		int peekRtn = curSession->_RecvRingQ.Peek((char*)&header, sizeof(LanHeader));

		if (header._Len <= 0)
		{
			//----------------------------------------
			// 헤더안에 표기된 Len이 0보다 같거나 작으면 역시 끊는다.
			//----------------------------------------
#if DISCONLOG_USE ==1
			_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"헤더의 Len :0  [Session ID:%llu] [Code:%d]", curSession->_ID, header._Len);
#endif
			Disconnect(curSession);
			return false;
		}
		if (usedSize - sizeof(LanHeader) < header._Len)
		{
			//-------------------------------------
			// 들어오려고하는패킷이, 내 링버퍼 현재 여유사이즈보다 크면 말이안되기때문에,
			// 그런 Session은 연결을 끊는다.
			//-------------------------------------
			if (header._Len > curSession->_RecvRingQ.GetFreeSize())
			{
#if DISCONLOG_USE ==1
				_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"RingQ사이즈보다 큰 패킷이들어옴 [Session ID:%llu] [Header Len:%d] [My RingQ FreeSize:%d]", curSession->_ID, header._Len, curSession->_RecvRingQ.GetFreeSize());
#endif
				Disconnect(curSession);
				return false;
			}
			break;
		}
		curSession->_RecvRingQ.MoveFront(sizeof(header));


		packet = LanPacket::Alloc();
		/*InterlockedIncrement(&g_AllocMemoryCount);*/

		int deQRtn = curSession->_RecvRingQ.Dequeue((*packet).GetPayloadPtr(), header._Len);


		if (deQRtn != header._Len)
		{
			CRASH();
		}
		(*packet).MoveWritePos(deQRtn);

		if ((*packet).GetPayloadSize() == 0)
		{
			CRASH();
		}

		InterlockedIncrement((LONG*)&m_RecvTPS);
		OnRecv(packet);

	}

	RecvPost(curSession);

	return true;
}

bool LanClient::RecvPost(Session* curSession)
{
	//SendFlag_Log sendFlagLog;
	//-------------------------------------------------------------
	// Recv 걸기
	//-------------------------------------------------------------
	if (curSession->_RecvRingQ.GetFreeSize() <= 0)
	{
#if DISCONLOG_USE ==1
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"RecvPost시, 링버퍼 여유사이즈없음 [Session ID:%llu] ", curSession->_ID);
#endif


		Disconnect(curSession);

		return false;
	}
	if (curSession->_IOCount <= 0)
	{
		CRASH();
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
		CRASH();
	}
	DWORD flag = 0;

	if (curSession->_IOCount <= 0)
	{
		CRASH();
	}

	InterlockedIncrement(&curSession->_IOCount);
#if IOCOUNT_CHECK ==1
	InterlockedIncrement(&g_IOPostCount);
	InterlockedIncrement(&g_IOIncDecCount);
#endif




	ZeroMemory(&curSession->_RecvOL, sizeof(curSession->_RecvOL));

	if (curSession->_IOCount <= 0)
	{
	
		CRASH();
	}

	//------------------------------------------------------------------
	// 	   IO Cancel 이 실행됬다면, 입출력을 걸지않고, IOCount를 낮추고 Return한다
	// 
	//------------------------------------------------------------------
	if (curSession->_bIOCancel)
	{
		if (0 == InterlockedDecrement(&curSession->_IOCount))
		{
			ReleaseSession(curSession);
		}

		return false;
	}
	int recvRtn = WSARecv(curSession->_Socket, wsaRecvBuf, bufCount, NULL, &flag, &curSession->_RecvOL, NULL);
	if (curSession->_IOCount <= 0)
	{
		CRASH();
	}
	//g_MemoryLog_IOCount.MemoryLogging(RECVPOST_RECVRTN, GetCurrentThreadId(), curSession->_ID, curSession->_IOCount, (int64_t)curSession->_Socket, (int64_t)curSession, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL);

	if (recvRtn == SOCKET_ERROR)
	{
		int errorCode = WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			curSession->_IOFail = true;
			int tempIOCount = InterlockedDecrement(&curSession->_IOCount);

			SpecialErrorCodeCheck(errorCode);
			if (0 == tempIOCount)
			{
				CRASH();
				ReleaseSession(curSession);
			}
			if (tempIOCount < 0)
			{
				CRASH();
			}
			return false;
		}
	}
	return true;
}


bool LanClient::SendPost()
{
	int loopCount = 0;

	if (!AcquireSession(m_Session))
	{
		//-------------------------------------------------
		// SendFlag진입전 미리 AcquireSession을 얻는다.
		// 실패하면 내부적으로 알아서 IOCount를 차감한다.
		// 성공한 후, SendFlag에 진입을 실패하면  현재 함수 루프밖에서 IOCount를 감소시킨다.
		//-------------------------------------------------
		return false;
	}
	do
	{
		loopCount++;

		if (0 == InterlockedExchange(&m_Session->_SendFlag, 1))
		{                  
			//--------------------------------------------------------
			// Echo Count가 증가한 범인
			//--------------------------------------------------------
			if (m_Session->_SendQ.GetQCount() <= 0)
			{
				InterlockedExchange(&m_Session->_SendFlag, 0);
				continue;
			}
			//--------------------------------------------------
			// IOCount와 이세션이 WSARecv or WSASend 이후 로그를 위해 Session에 접근할수있기 때문에
			// 참조카운트용으로 하나 더 증가시킨다.
			//--------------------------------------------------
			InterlockedIncrement(&m_Session->_IOCount);
			//--------------------------------------------------

			if (m_Session->_IOCount <= 0)
			{
				CRASH();
			}
			////-----------------------------------------------------------------------------------------------------------------------
			//// SendQ에 있는 LanPackt* 포인터들을 뽑아서 WSABUF를 세팅해준다
			////-----------------------------------------------------------------------------------------------------------------------
			WSABUF wsaSendBuf[Session::DEQ_PACKET_ARRAY_SIZE];

			int bufCount = 0;

			LanPacket* deQPacket = nullptr;

			if (m_Session->_DeQArraySize > 0)
			{
				CRASH();

			}
			while (m_Session->_SendQ.DeQ(&deQPacket))
			{
				if (deQPacket == nullptr)
				{
					CRASH();
				}
				if (m_Session->_DeQArraySize > Session::DEQ_PACKET_ARRAY_SIZE - 1)
				{
					CRASH();
				}
				
				if (deQPacket->GetPayloadSize() <= 0)
				{
					CRASH();
				}
				wsaSendBuf[m_Session->_DeQArraySize].buf = deQPacket->GetBufferPtr();
				wsaSendBuf[m_Session->_DeQArraySize].len = deQPacket->GetFullPacketSize();
				m_Session->_SendByte += wsaSendBuf[m_Session->_DeQArraySize].len;

				m_Session->_DeQPacketArray[m_Session->_DeQArraySize] = deQPacket;
				m_Session->_DeQArraySize++;
			}
			//------------------------------------------------------
			//   Send 송신바이트 체크하기
			//------------------------------------------------------
			if (m_Session->_SendByte <= 0)
			{
				CRASH();
			}

			ZeroMemory(&m_Session->_SendOL, sizeof(m_Session->_SendOL));

			if (m_Session->_IOCount <= 0)
			{
				CRASH();
			}

			//------------------------------------------------------------------
			// 	IO Cancel 이 실행됬다면, 입출력을 걸지않고, IOCount를 낮추고 Return한다
			//  Acquire IOCount +1 , 로그를위한 IOCount +1
			//------------------------------------------------------------------
			if (m_Session->_bIOCancel)
			{
				for (int i = 0; i < 2; ++i)
				{
					if (0 == InterlockedDecrement(&m_Session->_IOCount))
					{
						ReleaseSession(m_Session);
					}
				}
				return false;
			}


			int sendRtn = WSASend(m_Session->_Socket, wsaSendBuf, m_Session->_DeQArraySize, NULL, 0, &m_Session->_SendOL, NULL);


			if (m_Session->_IOCount <= 0)
			{
				CRASH();
			}

			if (sendRtn == SOCKET_ERROR)
			{
				int errorCode = WSAGetLastError();

				if (errorCode != WSA_IO_PENDING)
				{
					m_Session->_IOFail = true;
					SpecialErrorCodeCheck(errorCode);

					//---------------------------------------------------------
					// Acquire에서 얻은 IOCount를 감소시킨다.
					//---------------------------------------------------------
					int tempIOCount = InterlockedDecrement(&m_Session->_IOCount);

					if (0 == tempIOCount)
					{
						ReleaseSession(m_Session);
					}

				}
				else
				{
					/*	sendFlagLog.DataSettiong(InterlockedIncrement64(&m_SendFlagNo), eSendFlag::IO_PENDING, GetCurrentThreadId(), curSession->_Socket, (int64_t)curSession, (int64_t)&curSession->_RecvOL, (int64_t)&curSession->_SendOL, curSession->_SendFlag, curSession->_SendQ.GetQCount());
						curSession->_MemoryLog_SendFlag.MemoryLogging(sendFlagLog);*/
				}
			}

			//---------------------------------------------------------
			// Log를 위해 올렷던 IOCount를 감소시키고 끝낸다. (Return)
			//---------------------------------------------------------
			int tempIOCount = InterlockedDecrement(&m_Session->_IOCount);
			if (0 == tempIOCount)
			{
				ReleaseSession(m_Session);
			}
			return true;
		}
		else
		{
			break;
		}
	} while (m_Session->_SendQ.GetQCount() > 0);


	//-------------------------------------------------------------
	// 	   Acquire을 얻었지만, SendFlag를 못얻은경우 
	// 	   or
	// 	   Acquire도 얻고, SendFlag도 얻었지만 SendQ에 내용물이 없는경우
	//-------------------------------------------------------------
	int tempIOCount = InterlockedDecrement(&m_Session->_IOCount);
	if (0 == tempIOCount)
	{
		ReleaseSession(m_Session);
	}

	return true;

}

void LanClient::SpecialErrorCodeCheck(int32_t errorCode)
{
	if (errorCode != WSAECONNRESET && errorCode != WSAECONNABORTED && errorCode != WSAENOTSOCK && errorCode != WSAEINTR)
	{
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Special ErrorCode : %d", errorCode);
		CRASH();
	}
}

void LanClient::ReleaseSocket(Session* session)
{
#if MEMORYLOG_USE ==1 ||   MEMORYLOG_USE ==2
	IOCP_Log log;
#endif

	if (0 == InterlockedExchange(&session->_CloseFlag, 1))
	{
		//------------------------------------------------------
		// socket을 지우기전에, 먼저 session에있는 소켓부터 InvalidSocket으로 치환한다.
		//------------------------------------------------------


		SOCKET temp = session->_Socket;

		session->_Socket = INVALID_SOCKET;


		/*WSABUF wsaTempBuf;
		wsaTempBuf.buf = session->_RecvRingQ.GetRearBufferPtr();
		wsaTempBuf.len = session->_RecvRingQ.GetDirectEnqueueSize();

		DWORD flag = 0;


		int recvRtn = WSARecv(temp, &wsaTempBuf, 1, NULL, &flag, &session->_RecvOL, NULL);
		
		if (recvRtn == SOCKET_ERROR)
		{
			int errorCode = WSAGetLastError();
			if (errorCode == WSA_IO_PENDING)
			{
				CRASH();
			}
		}
		else
		{
			CRASH();
		}*/
		closesocket(temp);

	}
}



void LanClient::SessionClear(LanClient::Session* session)
{
	session->_Socket = INVALID_SOCKET;
	session->_CloseFlag = 0;

	//-------------------------------------------------
	// Release Flag 초기화 및 Accept 시 WSARecv에 걸 IOCount 미리 증가시킴
	//-------------------------------------------------
	InterlockedIncrement(&session->_IOCount);


	session->_SendFlag = 0;
	if (session->_SendQ.GetQCount() > 0)
	{
		CRASH();
	}

	session->_RecvRingQ.ClearBuffer();

	ZeroMemory(&session->_RecvOL, sizeof(session->_RecvOL));
	ZeroMemory(&session->_SendOL, sizeof(session->_SendOL));



	//-----------------------------------------
	// For Debug
	//-----------------------------------------
	//session->_MemoryLog_IOCP.Clear();
	//session->_MemoryLog_SendFlag.Clear();
	session->_SendByte = 0;
	session->_ErrorCode = 0;
	session->_GQCSRtn = TRUE;
	session->_TransferZero = 0;
	session->_IOFail = false;

	session->_bIOCancel = false;
}

void LanClient::DeQPacket(Session* session)
{

	for (int i = 0; i < session->_DeQArraySize; ++i)
	{
		LanPacket* delLanPacket = session->_DeQPacketArray[i];
		session->_DeQPacketArray[i] = nullptr;

		//_LOG(LOG_LEVEL_ERROR, L"DeQPacket Func Packet Decremetnt(%d->%d)", delLanPacket->m_RefCount, delLanPacket->m_RefCount - 1);
		if (0 == delLanPacket->DecrementRefCount())
		{
			delLanPacket->Free(delLanPacket);
		
		/*	InterlockedIncrement(&g_FreeMemoryCount);*/

		}

		//----------------------------------------------
		// Send 완료통지 후 처리되는것을 TPS로 카운팅 한다.
		//----------------------------------------------
		InterlockedIncrement(&m_SendTPS);
	}
	session->_DeQArraySize = 0;

}

void LanClient::ReleasePacket(Session* session)
{

	LanPacket* delLanPacket = nullptr;

	while (session->_SendQ.DeQ(&delLanPacket))
	{
		//_LOG(LOG_LEVEL_ERROR, L"Release  Packet Decremetnt(%d->%d)", delLanPacket->m_RefCount, delLanPacket->m_RefCount - 1);
		if (0 == delLanPacket->DecrementRefCount())
		{
			delLanPacket->Free(delLanPacket);
			//InterlockedIncrement(&g_FreeMemoryCount);
		}
	}

	//-----------------------------------------------------
	// SendPacket 후, SendPost까지했는데
	// 상대방이 연결을 끊은 경우 Send 완료통지가 오지않은경우 남아있을 수 있따
	// 이를 위해 처리를 해줘야된다.
	//-----------------------------------------------------
	if (session->_DeQArraySize > 0)
	{
		DeQPacket(session);
	}
}


bool LanClient::AcquireSession(Session* curSession)
{
	if (curSession == nullptr)
	{
		_LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Find Session이 널이다");
		CRASH();
		return false;;
	}

	InterlockedIncrement(&curSession->_IOCount);

	//----------------------------------------------------------
	// Release Flag가 true이거나 인자로 들어온 세션아이디와 현재 세션ID가다르면
	// IO를 하면 안된다.
	//----------------------------------------------------------
	if (curSession->_IOCount & 0x80000000)
	{
		if (0 == InterlockedDecrement(&curSession->_IOCount))
		{
			ReleaseSession(curSession);
		}

		return false;
	}

		

	return true;
}