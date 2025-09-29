#pragma once
#include "NetworkTask.h"


////////////////////////////////////////////////////////////////////
bool NetworkTaskClose::Run(bool result, DWORD transferred)
{
	for (auto sessionID : m_sessionIDs)
	{
		//auto session = NetworkSystem::GetInstance()->FindSession(sessionID);
		//if (session == nullptr) continue;

		////session 해제
		//session->Close();
	}

	return true;
}

bool NetworkTaskListen::Run(bool result, DWORD transferred)
{
	auto session = NetworkServer::GetInstance()->CreateNewSession(m_jobDispatcher);
	if (session == nullptr)
	{
		return false;
	}

	session->Listen(m_port);

	return false;
}
////////////////////////////////////////////////////////////////////
bool NetworkTaskChange::Run(bool result, DWORD transferred)
{
	//session 생성
	//auto session = NetworkSystem::GetInstance()->FindSession(m_sessionID);
	//if (session == nullptr)
	//{
	//	//LOG_ERR("failed change");
	//	return;
	//}

	//session->SetKey(m_key);

	return true;
}

////////////////////////////////////////////////////////////////////
bool NetworkTaskAcceptIO::Run(bool result, DWORD transferred)
{
	switch (m_step)
	{
	case EStep::None:		return Start(); 
	case EStep::Running:	return Complete(result);
	default: return false;
	}
}

bool NetworkTaskAcceptIO::Start()
{
	m_step = EStep::Running;
	if (m_owner == nullptr)
	{
		LOG_ERR("Session is null");
		return false;
	}

	//초기화
	ZeroMemory(static_cast<LPOVERLAPPED>(this), sizeof(OVERLAPPED));
	ZeroMemory(m_address, sizeof(m_address));

	//소켓생성
	m_clientSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (m_clientSocket == INVALID_SOCKET)
	{
		LOG_ERR("WSASocket failed:%", WSAGetLastError());
		return false;
	}
	//accept 요청
	DWORD bytes = 0;
	if (AcceptEx(m_owner->GetSocket(), m_clientSocket, m_address, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, static_cast<LPOVERLAPPED>(this)) == FALSE)
	{
		int32_t error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			LOG_ERR("AcceptEx failed:%", error);
			return false;
		}
	}

	return true;
}

bool NetworkTaskAcceptIO::Complete(bool result)
{
	if (m_owner == nullptr)
	{
		LOG_ERR("Session is null");
		return false;
	}

	//요청결과 체크
	if (result == true)
	{
		//SO_UPDATE_ACCEPT_CONTEXT
		SOCKET listener = m_owner->GetSocket();
		setsockopt(m_clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&listener, sizeof(listener));
		//접속주소 얻기
		sockaddr_in* local = nullptr;
		sockaddr_in* remote = nullptr;
		int32_t localLength = 0;
		int32_t remoteLength = 0;
		GetAcceptExSockaddrs((PVOID)m_address, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (sockaddr**)&local, &localLength, (sockaddr**)&remote, &remoteLength);
		if (remote)
		{
			auto task = new NetworkTaskNewUser;
			char ipStr[INET_ADDRSTRLEN];
			if (inet_ntop(AF_INET, &remote->sin_addr, ipStr, INET_ADDRSTRLEN) != nullptr)
			{
				std::cout << "IP: " << ipStr << std::endl;
				LOG_INFO("IP:%", ipStr);
			}
			else
			{
				LOG_ERR("InetNtop failed:");
			}
			task->m_ip = ipStr;
			task->m_port = static_cast<int32_t>(ntohs(remote->sin_port));
			task->m_socket = m_clientSocket;
			task->m_jobDispatcher = m_owner->GetJobDispatcher();
			NetworkServer::GetInstance()->WorkerPush(task);
		}
	}
	else
	{
		closesocket(m_clientSocket);
	}

	m_clientSocket = INVALID_SOCKET;

	//accept 요청
	return Start();
}

////////////////////////////////////////////////////////////////////
bool NetworkTaskNewUser::Run(bool result, DWORD transferred)
{
	//session 생성
	auto session = NetworkServer::GetInstance()->CreateNewSession(m_jobDispatcher);
	if (session == nullptr)
	{
		LOG_ERR("CreateSession is null");
		return false;
	}

	session->OnAccept(m_ip, m_port, m_socket);
	return false;
}

////////////////////////////////////////////////////////////////////
bool NetworkTaskReceiveIO::Run(bool result, DWORD transferred)
{
	switch (m_step)
	{
	case EStep::None:		return Start(); 
	case EStep::Running:	return Complete(result, transferred);
	default: return false;
	}
}

bool NetworkTaskReceiveIO::Start()
{
	m_step = EStep::Running;
	if (m_owner == nullptr)
	{
		LOG_ERR("Session is null");
		return false;
	}
	if (m_owner->_RecvRingQ.GetWriteSize() <= 0)
	{
		return false;
	}

	std::vector<WSABUF> wsaBufs{ };
	wsaBufs.reserve(2);
	m_owner->_RecvRingQ.GetDirectEnQData(wsaBufs);

	//receive 요청
	ZeroMemory(static_cast<LPOVERLAPPED>(this), sizeof(OVERLAPPED));
	DWORD bytes = 0;
	DWORD flag = 0;

	if (WSARecv(m_owner->GetSocket(), wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()), &bytes, &flag, static_cast<LPOVERLAPPED>(this), nullptr) == SOCKET_ERROR)
	{
		int32_t error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			LOG_ERR("failed - WSARecv:%", error);
			m_owner.reset();
			return false;
		}
	}

	return true;
}

bool NetworkTaskReceiveIO::Complete(bool result, DWORD transferred)
{
	if (m_owner == nullptr)
	{
		LOG_ERR("Session is null");
		return false;
	}

	//요청결과 체크
	if (result == false || transferred <= 0)
	{
		LOG_ERR("failed - result. transferred : % ", transferred);
		NetworkServer::GetInstance()->RemoveSession(m_owner->GetSessionID());
		m_owner.reset();
		return false;
	}

	//받은 사이즈 추가
	m_owner->_RecvRingQ.MoveWitePosition(transferred);

	//데이터 파싱
	while (true)
	{
		int packetSize = m_owner->_RecvRingQ.GetReadSize();
		if (packetSize >= PACKET_HEADER_SIZE)
		{
			char* packet = m_owner->_RecvRingQ.GetReadBufferPtr();
			int32_t messageSize = flatbuffers::ReadScalar<flatbuffers::uoffset_t>(packet);

			//데이터 사이즈 예외 체크
			if ( 0 <= messageSize && messageSize <= packetSize - PACKET_HEADER_SIZE)
			{
				//읽기 처리
				m_owner->_RecvRingQ.MoveReadPosition(PACKET_HEADER_SIZE + messageSize);
				NetworkServer::GetInstance()->Convert(packet + PACKET_HEADER_SIZE, messageSize);
				m_owner->OnRecvMessage(packet , messageSize);
				continue;
			}
		}

		break;
	}

	//receive 요청
	return Start();
}


////////////////////////////////////////////////////////////////////
bool NetworkTaskSend::Run(bool result, DWORD transferred)
{
	switch (m_step)
	{
	case EStep::None:		return Start();
	case EStep::Running:	return Complete(result, transferred);
	default: return false;
	}
}

bool NetworkTaskSend::Start()
{
	m_step = EStep::Running;
	if (m_owner == nullptr)
	{
		return false;
	}

	//OVERLAPPED 초기화
	ZeroMemory(static_cast<LPOVERLAPPED>(this), sizeof(OVERLAPPED));
	std::vector<WSABUF> wsaBufs{ };
	wsaBufs.reserve(m_owner->GetSendQueue().GetQCount());
	
	m_totalSize = 0;

	if(m_owner->m_isSending.exchange(true) == false)
	{
		NetworkPacket* networkPacket = nullptr;
		while (m_owner->GetSendQueue().DeQ(&networkPacket) == true)
		{
			WSABUF wsabuf;
			wsabuf.buf = reinterpret_cast<char*>(networkPacket->m_buffer.data());
			wsabuf.len = static_cast<ULONG>(networkPacket->m_buffer.size());
			wsaBufs.emplace_back(wsabuf);
			m_totalSize += wsabuf.len;
		}
		//send 요청
		DWORD bytes = 0;
		DWORD flag = 0;
		if (WSASend(m_owner->GetSocket(), wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()), &bytes, flag, static_cast<LPOVERLAPPED>(this), nullptr) == SOCKET_ERROR)
		{
			int32_t error = WSAGetLastError();
			if (error != WSA_IO_PENDING)
			{
				LOG_ERR("failed - WSASend:%", error);
				m_owner.reset();
				return false;
			}
		}
	}

	return true;
}

bool NetworkTaskSend::Complete(bool result, DWORD transferred)
{
	if (m_owner == nullptr)
	{
		//이게 발생하면..애초에 구현을 잘못한것..로그만 남긴다
		return false;
	}

	m_owner->m_isSending.exchange(false);

	//요청결과 체크
	if (result == false || transferred <= 0)
	{
		LOG_ERR("failed - result");
		return false;
	}

	//다 보냈는지 체크
	if (transferred != m_totalSize)
	{
		LOG_ERR("transferred:%/%", transferred, m_totalSize);
		return false;
	}

	return false;
}

////////////////////////////////////////////////////////////////////
bool NetworkTaskConnect::Run(bool result, DWORD transferred)
{
	//session 생성
	auto session = NetworkServer::GetInstance()->CreateNewSession(m_jobDispatcher);
	if (session == nullptr)
	{
		LOG_ERR("CreateSession failed");
		return false;
	}

	session->Connect(m_ip, m_port);
	return true;
}

////////////////////////////////////////////////////////////////////
bool NetworkTaskConnectIO::Run(bool result, DWORD transferred)
{
	switch (m_step)
	{
	case EStep::None:		return Start();
	case EStep::Running:	return Complete(result);
	}
	return false;
}

bool NetworkTaskConnectIO::Start()
{
	m_step = EStep::Running;
	ZeroMemory((LPOVERLAPPED)this, sizeof(OVERLAPPED));

	//소켓생성
	SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (sock == INVALID_SOCKET)
	{
		LOG_ERR("failed - WSASocket:%", WSAGetLastError());
		return false;
	}

	m_owner->SetSocket(sock);

	//포트할당
	SOCKADDR_IN local = { };
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	if (bind(m_owner->GetSocket(),reinterpret_cast<SOCKADDR*>(&local), sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		LOG_ERR("failed - bind:%", WSAGetLastError());
		return false;
	}

	//주소변환
	auto ip = m_owner->GetIP();
	auto port = m_owner->GetPort();

	SOCKADDR_IN remote = { };
	remote.sin_family = AF_INET;
	remote.sin_port = htons((unsigned short)port);

	if (ip.size() > 0)
	{
		if (isalpha(ip[0]))
		{
			addrinfo* infos = nullptr;
			addrinfo hints = { };
			hints.ai_family = remote.sin_family;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;

			getaddrinfo(ip.c_str(), nullptr, &hints, &infos);
			if (infos)
			{
				memcpy(&remote, infos->ai_addr, infos->ai_addrlen);
				freeaddrinfo(infos);
			}
			else
			{
				inet_pton(remote.sin_family, "127.0.0.1", &remote.sin_addr);
			}
		}
		else
		{
			inet_pton(remote.sin_family, ip.c_str(), &remote.sin_addr);
		}
	}
	else
	{
		inet_pton(remote.sin_family, "127.0.0.1", &remote.sin_addr);
	}

	//connect 요청
	static LPFN_CONNECTEX ConnectEx = nullptr;
	if (ConnectEx == nullptr)
	{
		GUID guid = WSAID_CONNECTEX;
		DWORD bytes = 0;
		if (WSAIoctl(m_owner->GetSocket(), SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx, sizeof(ConnectEx), &bytes, nullptr, nullptr) != 0)
		{
			return false;
		}
	}

	if (ConnectEx(m_owner->GetSocket(), reinterpret_cast<sockaddr*>(&remote), sizeof(remote), nullptr, 0, nullptr, static_cast<LPOVERLAPPED>(this)) == FALSE)
	{
		int32_t error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			LOG_ERR("failed - WSASend:%", error);
			return false;
		}
	}

	return true;
}

bool NetworkTaskConnectIO::Complete(bool result)
{
	if (m_owner == nullptr)
	{
		return false;
	}

	//요청결과 체크
	if (result == false)
	{
		LOG_ERR("result failed");
		return false;
	}
	m_owner->OnConnected();
	return false;
}

