#pragma once

//------------------------------------------------------
// 0 : Non Log
// 1 : g_Log
// 2 : Session Log && g_Log 
#define MEMORYLOG_USE 0
//------------------------------------------------------
//------------------------------------------------------
// 0 : Non Log
// 1 : 로그 사용
#define DISCONLOG_USE 0
//------------------------------------------------------

#define SERVER_NAME L"MMOGameServer"

#include "Option.h"
//#include "RingBuffer.h"
//#include "SerializeBuffer.h"
//#include "MemoryLog.h"
#include <unordered_map>
//#include "LockFreeStack.h"
//#include "LockFreeQ.h"
#include "TemplateQ.h"


enum eSessionStatus
{
	NOT_USED,
	AUTH,
	AUTH_TO_GAME,
	GAME,
	AUTH_RELEASE,
	GAME_RELEASE,
	RELEASE
};


class MMOGameLib;

class MMOSession
{
public:
	enum
	{
		//------------------------------------
		// 한 세션이 한 섹터를 기준으로 몇개의 메시지를 받는지 추정해서 
		// 세팅해야된다.
		//------------------------------------
		DEQ_PACKET_ARRAY_SIZE = 1000

	};
	MMOSession()
		:_SendQ(50000) 
	{
		_Socket = INVALID_SOCKET;
		_IOCount = 1;
		_ID = 0;
		ZeroMemory(&_RecvOL, sizeof(WSAOVERLAPPED));
		ZeroMemory(&_SendOL, sizeof(WSAOVERLAPPED));
		_SendByte = 0;
		_USED = false;

		_SessionOrder = 0;
		_OrderIndex = 0;
		_Index = -1;
		_DeQArraySize = 0;
		_LastRecvTime = 0;
		//-------------------------------------------
		// Accept 전  timeOut 3초  Accept 후 timeOut 1분~5분
		//-------------------------------------------
		_TimeOut = 3000;
		_TransferZero = 0;

		_IOFail = false;
		_bIOCancel = false;
		_bReserveDiscon = false;
		_bReleaseReady = false;

		_SessionStatus = eSessionStatus::NOT_USED;

		_bSending = false;

	}
	DWORD _SessionStatus;

	SOCKET _Socket;
	uint64_t _ID;
	int64_t _Index;
	bool _USED;
	RingQ _RecvRingQ;
#if SMART_PACKET_USE ==0
	LockFreeQ<NetPacket*> _SendQ;
	TemplateQ<NetPacket*> _CompleteRecvPacketQ;
	NetPacket* _DeQPacketArray[DEQ_PACKET_ARRAY_SIZE];
#endif
#if SMART_PACKET_USE ==1
	LockFreeQ<SmartNetPacket> _SendQ;
	SmartNetPacket _DeQPacketArray[DEQ_PACKET_ARRAY_SIZE];
#endif
	int32_t _DeQArraySize;
	WSAOVERLAPPED _RecvOL;
	WSAOVERLAPPED _SendOL;
	DWORD _IOCount;
	DWORD _CloseFlag;
	LONG _SendFlag;
	bool _bIOCancel;
	bool _bReleaseReady;
	bool _bSending;

	DWORD _SendByte;
	DWORD _LastRecvTime;
	DWORD _TimeOut;
	WCHAR _IP[17];
	uint16_t _Port;

	//------------------------------------------
	// For Debug
	//------------------------------------------
	bool _bReserveDiscon; // 더미클라이언트에선, 끊기전에 특정메시지를 보낸다 만약 이메시지를 보내지않았는데 Release를 하면 잘못된것
	uint64_t _SessionOrder;
	uint64_t _OrderIndex;
	uint64_t _LastSessionID[3][3];
	int _ErrorCode;
	BOOL _GQCSRtn;
	bool _IOFail;

	int _TransferZero;  //5 Recv 0  6 Send 0


public:
	//------------------------------------------
	// Contentes
	//------------------------------------------
	virtual void OnClientJoin_Auth(WCHAR* ip, uint16_t port) = 0;
	virtual void OnClientLeave_Auth() = 0;
	virtual void OnAuthPacket(NetPacket* packet) = 0;


	virtual void OnClientJoin_Game(WCHAR* ip, uint16_t port) = 0;
	virtual void OnClientLeave_Game() = 0;
	virtual void OnGamePacket(NetPacket* packet) = 0;

	virtual void OnError(int errorcode, WCHAR* errorMessage) = 0;
	virtual void OnTimeOut() = 0;

public:
	
	bool Disconnect();
	void IO_Cancel();

	//---------------------------------------------------
	// Send관련함수
	//---------------------------------------------------
	bool SendPost();
	bool SendPacket(NetPacket* packet);
	void SendUnicast(NetPacket* packet);


};

class MMOGameLib
{
public:

	struct ConnectInfo
	{
		SOCKET _Socket;
		WCHAR _IP[17];
		uint16_t _Port;
	};

public:
	MMOGameLib();
	~MMOGameLib();

public:
	bool ServerStart(WCHAR* ip, uint16_t port, DWORD runningThread, SocketOption& option, DWORD workerThreadCount, DWORD maxUserCount, TimeOutOption& timeOutOption, MMOSession** sessionArray);
	void ServerStop();
	bool NetworkInit(WCHAR* ip, uint16_t port, DWORD runningThread, SocketOption option);
	bool EventInit();
	bool ThreadInit(DWORD workerThreadCount);

	bool DisconnectAllUser();

	static void SpecialErrorCodeCheck(int32_t errorCode);


	uint64_t GetSessionID(uint64_t index);
	uint16_t GetSessionIndex(uint64_t sessionID);

	//-----------------------------------------------------------------------------
	// Release 관련 정리함수들
	//-----------------------------------------------------------------------------
	void ReleaseSession(MMOSession* delSession);
	void ReleaseSocket(MMOSession* session);
	void ReleasePacket(MMOSession* session);
	void DeQPacket(MMOSession* session);
	void SessionClear(MMOSession* session);
	

	//-----------------------------------------------------------------------------
	// Timeout, 보내고 끊기 함수들
	//-----------------------------------------------------------------------------
	void SetTimeOut(MMOSession* session);
	void SetTimeOut(MMOSession* session, DWORD timeOut);
	void SendNDiscon(MMOSession* session, NetPacket* packet);

public:

	virtual bool OnConnectionRequest(WCHAR* ip, uint16_t port) = 0;
	void AcceptUser(SOCKET socket, WCHAR* ip, uint16_t port);
	void CreateNewSession(ConnectInfo* conInfo);


	bool RecvPacket(MMOSession* curSession, DWORD transferByte);
	bool RecvPost(MMOSession* curSession);

	//-----------------------------------------------------------------------------
	// 모니터링 Getter
	//-----------------------------------------------------------------------------
	int64_t GetAcceptCount();
	LONG GetAcceptTPS();
	LONG GetSendTPS();
	LONG GetRecvTPS();
	LONG GetNetworkTraffic();
	LONG GetNetworkRecvTraffic();
	LONG GetSessionCount();
	int32_t GetMemoryAllocCount();
	LONG GetSendQMeomryCount();
	LONG GetLockFreeStackMemoryCount();
	DWORD GetAuthFPS();
	DWORD GetGameFPS();
	DWORD GetSendFPS();

protected:

	void SetSessionArray(MMOSession* sessionArray,DWORD maxUserCount);

private:
	//-----------------------------------------------------------------------
	// MMOG GameLib에서 관리하는 스레드 
	//-----------------------------------------------------------------------
	static unsigned int __stdcall AcceptThread(LPVOID param);
	static unsigned int __stdcall WorkerThread(LPVOID param);
	static unsigned int __stdcall MonitorThread(LPVOID param);
	static unsigned int __stdcall GameLogicThread(LPVOID param);
	static unsigned int __stdcall AuthThread(LPVOID param);
	static unsigned int __stdcall SendThread(LPVOID param);


public:
	static void Crash();
private:
	LockFreeStack<uint64_t>* m_IndexStack;
	

	TimeOutOption m_TimeOutOption;
	MemoryPool_TLS<ConnectInfo> m_ConInfoPool;
	TemplateQ<ConnectInfo*> m_SocketQ;

	uint64_t m_SessionID;
protected:
	DWORD m_MaxUserCount;
	LONG m_SessionCount;
	MMOSession** m_SessionArray;

	LONG m_AuthSessionCount;
	LONG m_GameSessionCount;


private:

	SOCKET m_ListenSocket;
	HANDLE* m_WorkerThread;
	HANDLE m_AcceptThread;
	HANDLE m_MonitoringThread;
	HANDLE m_GameThread;
	HANDLE m_AuthThread;
	HANDLE m_SendThread;

	uint16_t m_ServerPort;
	WCHAR* m_ServerIP;


	DWORD m_WorkerThreadCount;

	HANDLE m_IOCP;

	//------------------------------------------------
	// For Debugging
	//------------------------------------------------
	DWORD m_AuthFPS_To_Main;
	DWORD m_GameFPS_To_Main;
	DWORD m_SendFPS_To_Main;

	LONG m_NetworkTraffic;
	LONG m_NetworkRecvTraffic;

	//MyMemoryLog<int64_t> m_MemoryLog_Overlap;

	int64_t m_SendFlagNo;

	LONG m_RecvTPS;
	LONG m_SendTPS;
	LONG m_AcceptTPS;


	LONG m_RecvTPS_To_Main;
	LONG m_SendTPS_To_Main;
	LONG m_AcceptTPS_To_Main;
	LONG m_NetworkTraffic_To_Main;
	LONG m_NetworkRecvTraffic_To_Main;

	LONG m_SendQMemory;


	int64_t m_AcceptCount;

	MMOGameLib* m_Contents;

	HANDLE m_MonitorEvent;
	HANDLE m_GameThreadEvent;
	HANDLE m_AuthTreadEvent;
	HANDLE m_SendThreadEvent;

	MyLock m_PrintLock;




};