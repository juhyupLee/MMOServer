

bool NetworkSession::Disconnect()
{
	IO_Cancel();
	return true;
}

void NetworkSession::IO_Cancel()
{
	//-----------------------------------------------
	// Overlapped Pointer가 NULL일시 Send ,Recv 둘다 IO취소한다
	//-----------------------------------------------
	//CancelIoEx((HANDLE)_Socket, NULL);
}

void NetworkSession::Listen(int32_t port)
{
	SocketOption sockOption;
	sockOption._KeepAliveOption.onoff = 0;
	sockOption._Linger = true;
	sockOption._TCPNoDelay = false;
	sockOption._SendBufferZero = false;

	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
	{
		//LOG_ERR("failed - WSASocket:%", WSAGetLastError());
		return;
	}

	if (sockOption._SendBufferZero)
	{
		//----------------------------------------------------------------------------
		// 송신버퍼 Zero -->비동기 IO 유도
		//----------------------------------------------------------------------------
		int optVal = 0;
		int optLen = sizeof(optVal);

		int rtnOpt = setsockopt(listenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&optVal, optLen);
		if (rtnOpt != 0)
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}
	}
	if (sockOption._Linger)
	{
		linger lingerOpt;
		lingerOpt.l_onoff = 1;
		lingerOpt.l_linger = 0;

		int rtnOpt = setsockopt(listenSocket, SOL_SOCKET, SO_LINGER, (const char*)&lingerOpt, sizeof(lingerOpt));
		if (rtnOpt != 0)
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}

	}
	if (sockOption._TCPNoDelay)
	{
		BOOL tcpNodelayOpt = true;

		int rtnOpt = setsockopt(listenSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&tcpNodelayOpt, sizeof(tcpNodelayOpt));
		if (rtnOpt != 0)
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}
	}

	if (sockOption._KeepAliveOption.onoff)
	{
		DWORD recvByte = 0;

		if (0 != WSAIoctl(listenSocket, SIO_KEEPALIVE_VALS, &sockOption._KeepAliveOption, sizeof(tcp_keepalive), NULL, 0, &recvByte, NULL, NULL))
		{
			_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"setsockopt() error:%d", WSAGetLastError());
		}
	}


	NetworkServer::GetInstance()->RegisterSocketToIOCP(listenSocket);
	SetSocket(listenSocket);

	SOCKADDR_IN local = { };
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons((unsigned short)port);
	if (bind(listenSocket, (SOCKADDR*)&local, sizeof(local)) == SOCKET_ERROR)
	{
		//LOG_ERR("failed - bind:%", WSAGetLastError());
		return;
	}

	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR )
	{
		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"listen() error:%d", WSAGetLastError());
		return;
	}

	for (int32_t n = 0; n < 1; ++n)
	{
		auto task = new NetworkTaskAcceptIO;
		task->m_owner = shared_from_this();
		if (!NetworkServer::GetInstance()->WorkerPush(task))
		{
			/*LOG_ERR("fail - NetworkTaskAcceptIO WorkerPush");
			m_tasks.pop_back();
			Close();*/
			return;
		}
	}
}

void NetworkSession::SetSocket(SOCKET socket)
{
	m_socket = socket;
}

SOCKET NetworkSession::GetSocket()
{
	return m_socket;
}

JobDispatcher* NetworkSession::GetJobDispatcher()
{
	return m_jobQueue->GetJobDispatcher();
}

void NetworkSession::OnAccept(std::string ip, int32_t port, SOCKET sock)
{
	m_ip = ip;
	m_port = port;

	////타임아웃 적용하도록 셋팅
	//m_ignoreTimeout = false;

	//int64_t currentTime = TimeUtil::GetEpochTimeForNetwork();
	//m_timeoutTime = currentTime + NETWORK_TIMEOUT_TIME;
	//m_pingTime = currentTime + NETWORK_PING_TIME;

	//소켓셋팅
	SetSocket(sock);

	//내부로 접속한 유저가 있다고 알려준다.
	//FAcceptAckT msg1;
	//msg1.ip = ip;
	//msg1.port = port;
	//ExecuteCallback((HANDLE)m_sessionID, msg1);

	//접속한사람한테 알려준다.
	//FConnectAckT msg2;
	//msg2.result = EResultID::R_SUCCESS;
	//NetworkSystem::GetInstance()->Send(m_sessionID, msg2);

	//receive 요청
	auto task = new NetworkTaskReceiveIO;
	task->m_owner = this->shared_from_this();
	NetworkServer::GetInstance()->WorkerPush(task);

	//if (!NetworkSystem::GetInstance()->WorkerPush((LPOVERLAPPED)task.get()))
	//{
	//	LOG_ERR("fail - NetworkTaskReceiveIO WorkerPush");
	//	m_tasks.pop_back();
	//	Close();
	//	return;
	//}
}

//
//bool NetworkSession::SendPost()
//{
//	int loopCount = 0;
//
//	do
//	{
//		loopCount++;
//
//		if (0 == InterlockedExchange(&_SendFlag, 1))
//		{
//			//--------------------------------------------------------
//			// Echo Count가 증가한 범인
//			//--------------------------------------------------------
//			if (_SendQ.GetQCount() <= 0)
//			{
//				InterlockedExchange(&_SendFlag, 0);
//				continue;
//			}
//			//--------------------------------------------------
//			// IOCount와 이세션이 WSARecv or WSASend 이후 로그를 위해 Session에 접근할수있기 때문에
//			// 참조카운트용으로 하나 더 증가시킨다.
//			//--------------------------------------------------
//			InterlockedAdd((LONG*)&_IOCount, 2);
//			//--------------------------------------------------
//
//			if (_IOCount <= 0)
//			{
//				CRASH();
//			}
//
//	/*		if (_SessionStatus == eSessionStatus::RELEASE)
//			{
//				CRASH();
//			}*/
//
//			////-----------------------------------------------------------------------------------------------------------------------
//			//// SendQ에 있는 LanPackt* 포인터들을 뽑아서 WSABUF를 세팅해준다
//			////-----------------------------------------------------------------------------------------------------------------------
//			WSABUF wsaSendBuf[NetworkSession::DEQ_PACKET_ARRAY_SIZE];
//
//			int bufCount = 0;
//
//			NetPacket* deQPacket = nullptr;
//
//			if (_DeQArraySize > 0)
//			{
//				CRASH();
//
//			}
//			while (_SendQ.DeQ(&deQPacket))
//			{
//				if (deQPacket == nullptr)
//				{
//					CRASH();
//				}
//				if (_DeQArraySize > NetworkSession::DEQ_PACKET_ARRAY_SIZE - 1)
//				{
//					CRASH();
//				}
//
//				if (deQPacket->GetPayloadSize() <= 0)
//				{
//					CRASH();
//				}
//				wsaSendBuf[_DeQArraySize].buf = deQPacket->GetBufferPtr();
//				wsaSendBuf[_DeQArraySize].len = deQPacket->GetFullPacketSize();
//				_SendByte += wsaSendBuf[_DeQArraySize].len;
//
//				_DeQPacketArray[_DeQArraySize] = deQPacket;
//				_DeQArraySize++;
//			}
//			//------------------------------------------------------
//			//   Send 송신바이트 체크하기
//			//------------------------------------------------------
//			if (_SendByte <= 0)
//			{
//				CRASH();
//			}
//
//			ZeroMemory(&_SendOL, sizeof(_SendOL));
//
//			if (_IOCount <= 0)
//			{
//				CRASH();
//			}
//
//			//------------------------------------------------------------------
//			// 	IO Cancel 이 실행됬다면, 입출력을 걸지않고, IOCount를 낮추고 Return한다
//			//  로그를위한 IOCount +1  WSASend를 위한 +1 
//			//------------------------------------------------------------------
//			if (_bIOCancel)
//			{
//				for (int i = 0; i < 2; ++i)
//				{
//					if (0 == InterlockedDecrement(&_IOCount))
//					{
//						_bReleaseReady = true;
//					}
//				}
//
//				return false;
//			}
//
//
//			int sendRtn = WSASend(_Socket, wsaSendBuf, _DeQArraySize, NULL, 0, &_SendOL, NULL);
//
//			if (_IOCount <= 0)
//			{
//				CRASH();
//			}
//
//			if (sendRtn == SOCKET_ERROR)
//			{
//				int errorCode = WSAGetLastError();
//
//				if (errorCode != WSA_IO_PENDING)
//				{
//					_IOFail = true;
//					//MMOGameLib::SpecialErrorCodeCheck(errorCode);
//
//					//---------------------------------------------------------
//					// WSASend를 걸기위해 증가시킨 IOCount를 감소시킨다.
//					//---------------------------------------------------------
//					int tempIOCount = InterlockedDecrement(&_IOCount);
//
//					if (0 == tempIOCount)
//					{
//						_bReleaseReady = true;
//					}
//				}
//			}
//
//			//---------------------------------------------------------
//			// Log를 위해 올렷던 IOCount를 감소시키고 끝낸다. (Return)
//			//---------------------------------------------------------
//			int tempIOCount = InterlockedDecrement(&_IOCount);
//			if (0 == tempIOCount)
//			{
//				_bReleaseReady = true;
//			}
//			return true;
//		}
//		else
//		{
//			break;
//		}
//
//
//	} while (_SendQ.GetQCount() > 0);
//
//	return true;
//
//}

//bool NetworkSession::SendPacket(NetPacket* packet)
//{
//	(*packet).HeaderSettingAndEncoding();
//
//	if (packet->GetPayloadSize() <= 0)
//	{
//		CRASH();
//	}
//	if (!_SendQ.EnQ(packet))
//	{
//		//_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"SendQ 총갯수 초과(LockFreeQ Qcount 초과함)");
//		CRASH();
//	}
//
//	return true;
//}

//void NetworkSession::SendUnicast(NetPacket* packet)
//{
//	packet->IncrementRefCount();
//
//	if (!SendPacket(packet))
//	{
//		if (packet->DecrementRefCount() == 0)
//		{
//			packet->Free(packet);
//		}
//		return;
//	}
//
//	if (packet->DecrementRefCount() == 0)
//	{
//		packet->Free(packet);
//	}
//}