#include "PlayServer.h"

PlayServer::PlayServer()
	:m_main([this](int64_t key, MessageHolderPtr message)
	{
		Dispatch(key, message);
	}, 1)
{
	RegisterPacket(&PlayServer::OnFAuthenticationReq);
}

void PlayServer::OnFAuthenticationReq(int64_t sessionID, std::shared_ptr<FAuthenticationReqT>& msg)
{
	FAuthenticationAckT ack;
	ack.seq = msg->seq;

	auto token = msg->accounttoken;
	if (token.empty())
	{
		//LOG_ERR("empty token - %", sessionID);
		//Failure(sessionID, ack, EResultID::R_WRONG_TOKEN);
		return;
	}

	//std::regex pattern("^[a-zA-Z0-9]+$");
	//if (!std::regex_match(token, pattern))
	//{
	//	LOG_ERR("wrong token - %", sessionID);
	//	Failure(sessionID, ack, EResultID::R_WRONG_TOKEN);
	//	return;
	//}

	//SAuthenticationReqT dbreq;
	//dbreq.seq = msg->seq;
	//dbreq.sessionID = sessionID;
	//dbreq.accountToken = msg->accounttoken;
	//dbreq.reconnect = msg->reconnect;
	//dbreq.connectSessionKey = msg->connectSessionKey;
	//PushToDB(dbreq);
}

bool PlayServer::Initialize()
{
	NetworkServer::GetInstance()->Initialize();
	return true;
}

bool PlayServer::Start()
{
	NetworkServer::GetInstance()->Listen(7777, &m_main);
	return true;
}

void PlayServer::Run()
{
	//m_main.Run([]()
	//{
	//	
	//});
}

void PlayServer::Release()
{
}


void PlayServer::Dispatch(int64_t key, MessageHolderPtr& messageHolder)
{
	auto it = m_handlers.find(messageHolder->message.type);
	if (it != m_handlers.end())
	{
		(it->second)(key, messageHolder);
		return;
	}

	//if (m_proxySession.Dispatch(key, messageHolder) == true)
	//	return;

	//if (messageHolder->accountID.empty())
	//	return;

	//auto player = PlayerManager::GetInstance()->FindPlayer(messageHolder->accountID[0]);
	//if (player)
	//{
	//	player->Dispatch(messageHolder);
	//}
}