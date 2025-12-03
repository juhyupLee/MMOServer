#include "MessageJob.h"

NetworkSession::NetworkSession(JobDispatcher* jobDispatcher)
	:m_sessionID(UIDGenerator::GetInstance()->GenerateSessionID())
{
	m_jobQueue = MakeMySharedPtr<JobQueue>(jobDispatcher);
}

bool NetworkSession::Disconnect()
{
	closesocket(m_socket);
	return true;
}

int64_t NetworkSession::GetSessionID()
{
	return m_sessionID;
}

void NetworkSession::Listen(int32_t port)
{
	SocketOption sockOption;
	sockOption._KeepAliveOption.onoff = 0;
	sockOption._Linger = true;
	sockOption._TCPNoDelay = true;
	sockOption._SendBufferZero = false;

	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
	{
		LOG_ERR("failed - WSASocket:%", WSAGetLastError());
		return;
	}

	if (sockOption._SendBufferZero)
	{
		//----------------------------------------------------------------------------
		// 송신버퍼 Zero -->비동기 IO 유도
		//----------------------------------------------------------------------------
		int optVal = 0;
		int optLen = sizeof(optVal);

		int rtnOpt = setsockopt(listenSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&optVal), optLen);
		if (rtnOpt != 0)
		{
			LOG_ERR("failed - SetSockopt:%", WSAGetLastError());
		}
	}
	if (sockOption._Linger)
	{
		linger lingerOpt;
		lingerOpt.l_onoff = 1;
		lingerOpt.l_linger = 0;

		int rtnOpt = setsockopt(listenSocket, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&lingerOpt), sizeof(lingerOpt));
		if (rtnOpt != 0)
		{
			LOG_ERR("failed - SetSockopt:%", WSAGetLastError());
		}

	}
	if (sockOption._TCPNoDelay)
	{
		BOOL tcpNodelayOpt = true;

		int rtnOpt = setsockopt(listenSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&tcpNodelayOpt), sizeof(tcpNodelayOpt));
		if (rtnOpt != 0)
		{
			LOG_ERR("failed - SetSockopt:%", WSAGetLastError());
		}
	}

	if (sockOption._KeepAliveOption.onoff)
	{
		DWORD recvByte = 0;

		if (0 != WSAIoctl(listenSocket, SIO_KEEPALIVE_VALS, &sockOption._KeepAliveOption, sizeof(tcp_keepalive), NULL, 0, &recvByte, NULL, NULL))
		{
			LOG_ERR("failed - SetSockopt:%", WSAGetLastError());
		}
	}

	SetSocket(listenSocket);

	SOCKADDR_IN local = { };
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(static_cast<unsigned short>(port));
	if (bind(listenSocket, reinterpret_cast<SOCKADDR*>(&local) , sizeof(local)) == SOCKET_ERROR)
	{
		LOG_ERR("failed - bind:%", WSAGetLastError());
		return;
	}

	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR )
	{
		LOG_ERR("Listen Failed - bind:%", WSAGetLastError());
		return;
	}

	for (int32_t n = 0; n < 1; ++n)
	{
		auto task = new NetworkTaskAcceptIO;
		task->m_owner = shared_from_this();
		if (!NetworkServer::GetInstance()->WorkerPush(task))
		{
			return;
		}
	}
}

void NetworkSession::Connect(std::string ip, int32_t port)
{
	m_ip = ip;
	m_port = port;

	auto task = new NetworkTaskConnectIO;
	task->m_owner = shared_from_this();
	NetworkServer::GetInstance()->WorkerPush(task);
}

void NetworkSession::OnConnected()
{
	//receive 요청
	auto task = new NetworkTaskReceiveIO;
	task->m_owner = shared_from_this();
	NetworkServer::GetInstance()->WorkerPush(task);
}

void NetworkSession::SetSocket(SOCKET socket)
{
	NetworkServer::GetInstance()->RegisterSocketToIOCP(socket);
	m_socket = socket;
}

SOCKET NetworkSession::GetSocket()
{
	return m_socket;
}

std::string NetworkSession::GetIP()
{
	return m_ip;
}

int32_t NetworkSession::GetPort()
{
	return m_port;
}

JobDispatcher* NetworkSession::GetJobDispatcher()
{
	return m_jobQueue->GetJobDispatcher();
}

LockFreeQ<NetworkPacket*>& NetworkSession::GetSendQueue()
{
	return m_sendQueue;
}

RingQ& NetworkSession::GetRecvRingQueue()
{
	return m_recvRingQueue;
}

void NetworkSession::OnAccept(std::string ip, int32_t port, SOCKET sock)
{
	m_ip = ip;
	m_port = port;

	////타임아웃 적용하도록 셋팅

	//소켓셋팅
	SetSocket(sock);

	FConnectAckT msg2;
	msg2.result = EResultID::R_SUCCESS;
	Send(msg2);
	//Send(msg);
	//내부로 접속한 유저가 있다고 알려준다.

	//접속한사람한테 알려준다.

	//receive 요청
	auto task = new NetworkTaskReceiveIO;
	task->m_owner = this->shared_from_this();
	NetworkServer::GetInstance()->WorkerPush(task);
}

void NetworkSession::OnRecvMessage(char* messageBuffer, int32_t messageSize)
{
	flatbuffers::Verifier verifier((uint8_t*)messageBuffer, PACKET_HEADER_SIZE + messageSize);
	if (VerifySizePrefixedMessageHolderBuffer(verifier) == false)
	{
		return;
	}
	const auto packedMessageHolder = GetSizePrefixedMessageHolder(messageBuffer);
	const auto messageID = packedMessageHolder->message_type();

	const auto messageHolder = MessageHolderPtr(packedMessageHolder->UnPack());
	m_jobQueue->Push(MakeMySharedPtr<MessageJob>(messageHolder, m_sessionID));
}
