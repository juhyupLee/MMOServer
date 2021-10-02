#pragma once
#include "MMOGameLib.h"

class MonitorLanClient;

class Client : public MMOSession
{

public:
	Client()
	{
		_AccountNo = -1;
		_Version = -1;
	}
	//------------------------------------------
	// Contentes
	//------------------------------------------
	virtual void OnClientJoin_Auth(WCHAR* ip, uint16_t port);
	virtual void OnClientLeave_Auth() ;
	virtual void OnAuthPacket(NetPacket* packet) ;


	virtual void OnClientJoin_Game(WCHAR* ip, uint16_t port) ;
	virtual void OnClientLeave_Game();
	virtual void OnGamePacket(NetPacket* packet) ;

	virtual void OnError(int errorcode, WCHAR* errorMessage);
	virtual void OnTimeOut();


	void PacketProc(NetPacket* packet);

	void PacketProcess_en_PACKET_CS_GAME_REQ_LOGIN(NetPacket* netPacket);
	void PacketProcess_en_PACKET_CS_GAME_REQ_ECHO(NetPacket* netPacket);
public:
	int64_t _AccountNo;
	WCHAR _NickName[20];
	char _SessionKey[64];
	int32_t _Version;

};

class MyMMOServer : public MMOGameLib
{
public:
	enum
	{
		MAX_IP_COUNT = 100,
		MAX_WHITE_IP_COUNT = 3

	};

	struct Black_IP
	{
		WCHAR _IP[17];
		uint16_t _Port;
	};
	struct White_IP
	{
		WCHAR _IP[17];
		uint16_t _Port;
	};

public:
	MyMMOServer();
	~MyMMOServer();
	void ServerMonitorPrint();
	void SendMonitorData();

public:
	bool MMOServerStart(WCHAR* ip, uint16_t port, DWORD runningThread, SocketOption& option, DWORD workerThreadCount, DWORD maxUserCount, TimeOutOption& timeOutOption);
	bool MMOServerStop();
public:
	virtual bool OnConnectionRequest(WCHAR* ip, uint16_t port);

private:
	Client* FindClient(uint64_t sessionID);
	

private:
	//--------------------------------------------
	// 	   모니터링관련 멤버변수
	//--------------------------------------------
	bool m_bMonitorServerLogin;
	bool m_bServerOn;
	MonitorLanClient* m_MonitorClient;
	//--------------------------------------------

	MemoryPool_TLS<Client> m_ClientPool;
	std::unordered_map<uint64_t, Client*> m_ClientMap;

	WCHAR m_BlackIPList[MAX_IP_COUNT][17];
	WCHAR m_WhiteIPList[MAX_WHITE_IP_COUNT][17];

	//---------------------------------------------------------
	// 	   For Debug
	//--------------------------------------------------------
	int64_t m_MaxTCPRetrans ;
};

