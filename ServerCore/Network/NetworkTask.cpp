#pragma once
#include "NetworkTask.h"


////////////////////////////////////////////////////////////////////
void NetworkTaskClose::Run(bool result, DWORD transferred)
{
	for (auto sessionID : m_sessionIDs)
	{
		//auto session = NetworkSystem::GetInstance()->FindSession(sessionID);
		//if (session == nullptr) continue;

		////session 해제
		//session->Close();
	}
}

void NetworkTaskListen::Run(bool result, DWORD transferred)
{
	auto session = NetworkServer::GetInstance()->CreateNewSession(m_jobDispatcher);
	if (session == nullptr)
	{
		return;
	}

	session->Listen(m_port);
}
////////////////////////////////////////////////////////////////////
void NetworkTaskChange::Run(bool result, DWORD transferred)
{
	//session 생성
	//auto session = NetworkSystem::GetInstance()->FindSession(m_sessionID);
	//if (session == nullptr)
	//{
	//	//LOG_ERR("failed change");
	//	return;
	//}

	//session->SetKey(m_key);
}

////////////////////////////////////////////////////////////////////
void NetworkTaskAcceptIO::Run(bool result, DWORD transferred)
{
	switch (m_step)
	{
	case EStep::None:		Start(); break;
	case EStep::Running:	Complete(result); break;
	}
}

void NetworkTaskAcceptIO::Start()
{
	m_step = EStep::Running;
	if (m_owner == nullptr)
	{
		//LOG_ERR("session null");
		//NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
		return;
	}

	//초기화
	ZeroMemory(&m_overlapped, sizeof(OVERLAPPED));
	ZeroMemory(m_address, sizeof(m_address));

	//소켓생성
	m_clientSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (m_clientSocket == INVALID_SOCKET)
	{
		//LOG_ERR("WSASocket failed:%", WSAGetLastError());
		//NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
		return;
	}
	//NetworkServer::GetInstance()->RegisterSocketToIOCP(m_clientSocket);

	//accept 요청
	DWORD bytes = 0;

	
	if (AcceptEx(m_owner->GetSocket(), m_clientSocket, m_address, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, reinterpret_cast<LPOVERLAPPED>(&m_overlapped)) == FALSE)
	{
		int32_t error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			//LOG_ERR("AcceptEx failed:%", error);
			//NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
			return;
		}
	}
}

void NetworkTaskAcceptIO::Complete(bool result)
{
	if (m_owner == nullptr)
	{
		//LOG_ERR("session null");
		//NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
		return;
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
			}
			else
			{
				std::cout << "InetNtop failed" << std::endl;
			}
			task->m_ip = ipStr;
			task->m_port = (int32_t)ntohs(remote->sin_port);
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
	Start();
}

////////////////////////////////////////////////////////////////////
void NetworkTaskNewUser::Run(bool result, DWORD transferred)
{
	//session 생성
	auto session = NetworkServer::GetInstance()->CreateNewSession(m_jobDispatcher);
	if (session == nullptr)
	{
//		LOG_ERR("CreateSession failed");
		return;
	}

	session->OnAccept(m_ip, m_port, m_socket);
}

////////////////////////////////////////////////////////////////////
void NetworkTaskReceiveIO::Run(bool result, DWORD transferred)
{
	//switch (m_step)
	//{
	//case EStep::None:		Start(); break;
	//case EStep::Running:	Complete(result, transferred); break;
	//}
}

void NetworkTaskReceiveIO::Start()
{
	//m_step = EStep::Running;

	//auto session = (NetworkSession*)m_owner;
	//if (session == nullptr)
	//{
	//	LOG_ERR("session null");
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	////버퍼가 없으면 생성
	//if (m_buffer.size() <= 0)
	//{
	//	m_writeSize = 0;
	//	m_readSize = 0;
	//	m_buffer.resize(DEFAULT_CAPACITY_SIZE);
	//}

	////읽기와 쓰기가 같은지 체크
	//if (m_readSize == m_writeSize)
	//{
	//	//버퍼 축소
	//	if (m_buffer.size() > DEFAULT_CAPACITY_SIZE)
	//	{
	//		m_buffer.resize(DEFAULT_CAPACITY_SIZE);
	//	}

	//	//읽기,쓰기 초기화
	//	m_writeSize = 0;
	//	m_readSize = 0;
	//}

	////빈공간 체크
	//int32_t emptySize = GetEmptySize();
	//if (emptySize < MIN_CAPACITY_SIZE)
	//{
	//	if (emptySize + m_readSize >= MIN_CAPACITY_SIZE)
	//	{
	//		//재정렬만 함
	//		memmove(m_buffer.data(), m_buffer.data() + m_readSize, m_writeSize - m_readSize);
	//		m_writeSize -= m_readSize;
	//		m_readSize = 0;
	//	}
	//	else
	//	{
	//		//최대 사이즈를 넘어가는지 체크
	//		if (m_buffer.size() >= MAX_CAPACITY_SIZE)
	//		{
	//			LOG_ERR("size over");
	//			NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//			return;

	//		}

	//		//새버퍼를 생성하여 바꿔치기
	//		std::vector<char> temp;
	//		temp.resize(m_buffer.size() + DEFAULT_CAPACITY_SIZE);

	//		memmove(temp.data(), m_buffer.data() + m_readSize, m_writeSize - m_readSize);
	//		m_writeSize -= m_readSize;
	//		m_readSize = 0;

	//		m_buffer.swap(temp);
	//	}
	//}

	////OVERLAPPED 초기화
	//ZeroMemory((LPOVERLAPPED)this, sizeof(OVERLAPPED));

	////receive 요청
	//WSABUF wsabuf = { };
	//DWORD bytes = 0;
	//DWORD flag = 0;

	//wsabuf.buf = GetEmpty();
	//wsabuf.len = GetEmptySize();

	//if (WSARecv(session->GetSocket(), &wsabuf, 1, &bytes, &flag, (LPOVERLAPPED)this, nullptr) == SOCKET_ERROR)
	//{
	//	int32_t error = WSAGetLastError();
	//	if (error != WSA_IO_PENDING)
	//	{
	//		LOG_ERR("failed - WSARecv:%", error);
	//		NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//		return;
	//	}
	//}
}

void NetworkTaskReceiveIO::Complete(bool result, DWORD transferred)
{
	//auto session = (NetworkSession*)m_owner;
	//if (session == nullptr)
	//{
	//	LOG_ERR("session null");
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	////요청결과 체크
	//if (result == false || transferred <= 0)
	//{
	//	//LOG_ERR("failed - result. transferred : % ", transferred);
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	////받은 사이즈 추가
	//m_writeSize += transferred;

	////데이터 파싱
	//while (true)
	//{
	//	char* packet = GetData();
	//	int32_t packetSize = GetDataSize();

	//	if (packetSize >= PACKET_HEADER_SIZE)
	//	{
	//		int32_t messageSize = flatbuffers::ReadScalar<flatbuffers::uoffset_t>(packet);

	//		//데이터 사이즈 예외 체크
	//		if (messageSize >= 0 && messageSize <= packetSize - PACKET_HEADER_SIZE)
	//		{
	//			//읽기 처리
	//			m_readSize += PACKET_HEADER_SIZE + messageSize;

	//			//받은 패킷 전달
	//			NetworkSystem::Convert(packet + PACKET_HEADER_SIZE, messageSize);
	//			session->Transfer(packet, messageSize);
	//			continue;
	//		}
	//	}

	//	break;
	//}

	////receive 요청
	//Start();
}

char* NetworkTaskReceiveIO::GetData()
{
	if (m_buffer.size() <= 0) return nullptr;
	if (m_buffer.size() < m_writeSize) return nullptr;
	if (m_writeSize <= 0 || m_writeSize <= m_readSize) return nullptr;

	return m_buffer.data() + m_readSize;
}

int32_t NetworkTaskReceiveIO::GetDataSize()
{
	/*if (m_buffer.size() <= 0) return 0;
	if (m_buffer.size() < m_writeSize) return 0;
	if (m_writeSize <= 0 || m_writeSize <= m_readSize) return 0;

	return m_writeSize - m_readSize;*/
	return 0;
}

char* NetworkTaskReceiveIO::GetEmpty()
{
	/*if (m_buffer.size() <= 0) return nullptr;
	if (m_buffer.size() <= m_writeSize) return nullptr;

	return m_buffer.data() + m_writeSize;*/

	return nullptr;
}

int32_t NetworkTaskReceiveIO::GetEmptySize()
{
	/*if (m_buffer.size() <= 0) return 0;
	if (m_buffer.size() <= m_writeSize) return 0;

	return (int32_t)m_buffer.size() - m_writeSize;*/

	return 0;
}

////////////////////////////////////////////////////////////////////
void NetworkTaskSend::Run(bool result, DWORD transferred)
{
	//m_step = EStep::Running;

	//while (m_serialized == false)
	//{
	//	std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
	//}

	//// session 체크
	//for (auto sessionID : m_sessionIDs)
	//{
	//	auto session = NetworkSystem::GetInstance()->FindSession(sessionID);
	//	if (session == nullptr) continue;

	//	//리스트에 추가
	//	session->Send(m_packet);
	//}
}

////////////////////////////////////////////////////////////////////
void NetworkTaskDirectSend::Run(bool result, DWORD transferred)
{
	//m_step = EStep::Running;

	//// session 체크
	//for (auto sessionID : m_sessionIDs)
	//{
	//	auto session = NetworkSystem::GetInstance()->FindSession(sessionID);
	//	if (session == nullptr) continue;

	//	//리스트에 추가
	//	session->Send(m_packet);
	//}
}

////////////////////////////////////////////////////////////////////
void NetworkTaskSendIO::Run(bool result, DWORD transferred)
{
	//switch (m_step)
	//{
	//case EStep::None:		Start(); break;
	//case EStep::Running:	Complete(result, transferred); break;
	//}
}

void NetworkTaskSendIO::Start()
{
	//m_step = EStep::Running;

	//auto session = (NetworkSession*)m_owner;
	//if (session == nullptr)
	//{
	//	LOG_ERR("session null");
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	////OVERLAPPED 초기화
	//ZeroMemory((LPOVERLAPPED)this, sizeof(OVERLAPPED));

	//m_wsabufs.reserve(m_packets.size());
	//m_totalSize = 0;

	//for (const auto& packet : m_packets)
	//{
	//	WSABUF wsabuf;
	//	wsabuf.buf = (char*)packet->m_buffer.data();
	//	wsabuf.len = (ULONG)packet->m_buffer.size();
	//	m_wsabufs.emplace_back(wsabuf);

	//	//패킷로그
	//	if (!packet->m_json.empty())
	//	{
	//		if (LogManager::GetInstance()->GetPacketLogActivate())
	//		{
	//			LogManager::GetInstance()->PrintPacket(std::format("[session:{}]", session->GetSessionID()), packet->m_json);
	//		}
	//	}

	//	// 보낼데이터의 총 사이즈 계산
	//	m_totalSize += wsabuf.len;
	//}

	////send 요청
	//DWORD bytes = 0;
	//DWORD flag = 0;

	//if (WSASend(session->GetSocket(), m_wsabufs.data(), (DWORD)m_wsabufs.size(), &bytes, flag, (LPOVERLAPPED)this, nullptr) == SOCKET_ERROR)
	//{
	//	int32_t error = WSAGetLastError();
	//	if (error != WSA_IO_PENDING)
	//	{
	//		LOG_ERR("failed - WSASend:%", error);
	//		NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//		return;
	//	}
	//}
}

void NetworkTaskSendIO::Complete(bool result, DWORD transferred)
{
	//auto session = (NetworkSession*)m_owner;
	//if (session == nullptr)
	//{
	//	//이게 발생하면..애초에 구현을 잘못한것..로그만 남긴다
	//	LOG_ERR("session null");
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	////요청결과 체크
	//if (result == false || transferred <= 0)
	//{
	//	LOG_ERR("failed - result");
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	////다 보냈는지 체크
	//if (transferred != m_totalSize)
	//{
	//	LOG_ERR("transferred:%/%", transferred, m_totalSize);
	//	return;
	//}

	////send 완료 처리
	//NetworkSystem::GetInstance()->Notify(EStep::SuccessSend, m_owner, this);
}

////////////////////////////////////////////////////////////////////
void NetworkTaskConnect::Run(bool result, DWORD transferred)
{
	////session 생성
	//auto session = NetworkSystem::GetInstance()->CreateSession(m_callback, m_proxyCallback);
	//if (session == nullptr)
	//{
	//	LOG_ERR("CreateSession failed");
	//	return;
	//}

	//session->Connect(m_ip, m_port);
}

////////////////////////////////////////////////////////////////////
void NetworkTaskConnectIO::Run(bool result, DWORD transferred)
{
	//switch (m_step)
	//{
	//case EStep::None:		Start(); break;
	//case EStep::Running:	Complete(result); break;
	//}
}

void NetworkTaskConnectIO::Start()
{
	//m_step = EStep::Running;

	//auto session = (NetworkSession*)m_owner;
	//if (session == nullptr)
	//{
	//	LOG_ERR("session null");
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	//ZeroMemory((LPOVERLAPPED)this, sizeof(OVERLAPPED));

	////소켓생성
	//SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	//if (sock == INVALID_SOCKET)
	//{
	//	LOG_ERR("failed - WSASocket:%", WSAGetLastError());
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	//if (session->SetSocket(sock) == false)
	//{
	//	LOG_ERR("SetSocket Failed");
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	////포트할당
	//SOCKADDR_IN local = { };
	//local.sin_family = AF_INET;
	//local.sin_addr.s_addr = INADDR_ANY;
	//if (bind(session->GetSocket(), (SOCKADDR*)&local, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	//{
	//	LOG_ERR("failed - bind:%", WSAGetLastError());
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	////주소변환
	//auto ip = session->GetIP();
	//auto port = session->GetPort();

	//SOCKADDR_IN remote = { };
	//remote.sin_family = AF_INET;
	//remote.sin_port = htons((unsigned short)port);

	//if (ip.size() > 0)
	//{
	//	if (isalpha(ip[0]))
	//	{
	//		addrinfo* infos = nullptr;
	//		addrinfo hints = { };
	//		hints.ai_family = remote.sin_family;
	//		hints.ai_socktype = SOCK_STREAM;
	//		hints.ai_protocol = IPPROTO_TCP;

	//		getaddrinfo(ip.c_str(), nullptr, &hints, &infos);
	//		if (infos)
	//		{
	//			memcpy(&remote, infos->ai_addr, infos->ai_addrlen);
	//			freeaddrinfo(infos);
	//		}
	//		else
	//		{
	//			inet_pton(remote.sin_family, "127.0.0.1", &remote.sin_addr);
	//		}
	//	}
	//	else
	//	{
	//		inet_pton(remote.sin_family, ip.c_str(), &remote.sin_addr);
	//	}
	//}
	//else
	//{
	//	inet_pton(remote.sin_family, "127.0.0.1", &remote.sin_addr);
	//}

	////connect 요청
	//static LPFN_CONNECTEX ConnectEx = nullptr;
	//if (ConnectEx == nullptr)
	//{
	//	GUID guid = WSAID_CONNECTEX;
	//	DWORD bytes = 0;
	//	if (WSAIoctl(session->GetSocket(), SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx, sizeof(ConnectEx), &bytes, nullptr, nullptr) != 0)
	//	{
	//		LOG_ERR("WSAIoctl failed:%", WSAGetLastError());
	//		NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//		return;
	//	}
	//}

	//if (ConnectEx(session->GetSocket(), (sockaddr*)&remote, sizeof(remote), nullptr, 0, nullptr, (LPOVERLAPPED)this) == FALSE)
	//{
	//	int32_t error = WSAGetLastError();
	//	if (error != WSA_IO_PENDING)
	//	{
	//		LOG_ERR("ConnectEx failed:%", error);
	//		NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//		return;
	//	}
	//}
}

void NetworkTaskConnectIO::Complete(bool result)
{
	//auto session = (NetworkSession*)m_owner;
	//if (session == nullptr)
	//{
	//	LOG_ERR("session null");
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	////요청결과 체크
	//if (result == false)
	//{
	//	//LOG_ERR("result failed");
	//	NetworkSystem::GetInstance()->Notify(EStep::Failed, m_owner, this);
	//	return;
	//}

	////시작 처리
	//NetworkSystem::GetInstance()->Notify(EStep::SuccessConnect, m_owner, this);
}

