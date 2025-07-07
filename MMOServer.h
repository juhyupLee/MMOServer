#pragma once
//#include "NetworkServer.h"
//#include "./ServerCore/Network/NetworkSession.h"
class MonitorLanClient;
class MyMMOServer : public NetworkServer
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
	

private:
	//--------------------------------------------
	// 	   모니터링관련 멤버변수
	//--------------------------------------------
	bool m_bMonitorServerLogin;
	bool m_bServerOn;
	MonitorLanClient* m_MonitorClient;
	//-------------------------------------------
	WCHAR m_BlackIPList[MAX_IP_COUNT][17];
	WCHAR m_WhiteIPList[MAX_WHITE_IP_COUNT][17];

	//---------------------------------------------------------
	// 	   For Debug
	//--------------------------------------------------------
	int64_t m_MaxTCPRetrans ;
};

