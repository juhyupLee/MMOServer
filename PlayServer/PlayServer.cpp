#include "PlayServer.h"

PlayServer::PlayServer()
	:m_main([this](int64_t key, PacketHolder message) {MainDispatch(key, message); }, 1),
     m_sub([this](int64_t key, PacketHolder message) {MainDispatch(key, message); }, 1)
{
	RegisterPacket(&PlayServer::ON_CLGS_AUTHEN_REQ);
}

void PlayServer::ON_CLGS_AUTHEN_REQ(int64_t sessionID, std::shared_ptr<CLGS_AUTHEN_REQT>& msg)
{
	CLGS_AUTHEN_ACKT ack;
	ack.seq = msg->seq;

	auto token = msg->accounttoken;
	if (token.empty())
	{
		return;
	}

	//DB검증 요청
}

bool PlayServer::Initialize()
{
	LogManager::GetInstance()->Init();
	NetworkServer::GetInstance()->Initialize();
	return true;
}

bool PlayServer::Start()
{
	NetworkServer::GetInstance()->Listen(7777, &m_main);
	NetworkServer::GetInstance()->Connect("127.0.0.1", 13001, &m_main);
	
	return true;
}

void PlayServer::Run()
{
	LOG_INFO("Server Start");
	while (true)
	{

	}
}

void PlayServer::Release()
{
}


void PlayServer::MainDispatch(int64_t key, PacketHolder& messageHolder)
{
	auto it = m_handlers.find(messageHolder->message.type);
	if (it != m_handlers.end())
	{
		(it->second)(key, messageHolder);
		return;
	}

	//if (m_proxySession.MainDispatch(key, messageHolder) == true)
	//	return;

	//if (messageHolder->accountID.empty())
	//	return;

	//auto player = PlayerManager::GetInstance()->FindPlayer(messageHolder->accountID[0]);
	//if (player)
	//{
	//	player->MainDispatch(messageHolder);
	//}
}