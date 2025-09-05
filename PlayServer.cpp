#include "PlayServer.h"
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
	m_main.Run([]()
	{
		
	});
}

void PlayServer::Release()
{
}
