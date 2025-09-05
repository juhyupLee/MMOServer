#pragma once
#include "JobQueue.h"

class NetworkSession : public std::enable_shared_from_this<NetworkSession>
{
public:
	NetworkSession(JobDispatcher* jobDispatcher)
	{
		m_jobQueue = std::make_shared<JobQueue>(jobDispatcher);
	}

	DWORD _SessionStatus;
	RingQ _RecvRingQ;

	int32_t m_port;
	std::string m_ip;
	std::shared_ptr<JobQueue> m_jobQueue;
	SOCKET m_socket;
	
public:
	//------------------------------------------
	// Contentes
	//------------------------------------------
	//virtual void OnClientJoin_Auth(WCHAR* ip, uint16_t port) = 0;
	//virtual void OnClientLeave_Auth() = 0;
	////virtual void OnAuthPacket(NetPacket* packet) = 0;


	//virtual void OnClientJoin_Game(WCHAR* ip, uint16_t port) = 0;
	//virtual void OnClientLeave_Game() = 0;
	////virtual void OnGamePacket(NetPacket* packet) = 0;

	//virtual void OnError(int errorcode, WCHAR* errorMessage) = 0;
	//virtual void OnTimeOut() = 0;

public:

	bool Disconnect();
	void IO_Cancel();

	//---------------------------------------------------
	// Send관련함수
	//---------------------------------------------------
	//bool SendPost();
	//bool SendPacket(NetPacket* packet);
	//void SendUnicast(NetPacket* packet);


	void Listen(int32_t port);
	void SetSocket(SOCKET socket);
	SOCKET GetSocket();

	JobDispatcher* GetJobDispatcher();
	void OnAccept(std::string ip, int32_t port, SOCKET sock);
};
