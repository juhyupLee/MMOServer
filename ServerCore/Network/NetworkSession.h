#pragma once
class NetworkSession
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
	NetworkSession()
		
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

		/*_SessionStatus = eSessionStatus::NOT_USED;*/

		_bSending = false;

	}
	DWORD _SessionStatus;

	SOCKET _Socket;
	uint64_t _ID;
	int64_t _Index;
	bool _USED;
	RingQ _RecvRingQ;
#if SMART_PACKET_USE ==0
	//LockFreeQ<NetPacket*> _SendQ;
	//TemplateQ<NetPacket*> _CompleteRecvPacketQ;
	//NetPacket* _DeQPacketArray[DEQ_PACKET_ARRAY_SIZE];
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
	//virtual void OnAuthPacket(NetPacket* packet) = 0;


	virtual void OnClientJoin_Game(WCHAR* ip, uint16_t port) = 0;
	virtual void OnClientLeave_Game() = 0;
	//virtual void OnGamePacket(NetPacket* packet) = 0;

	virtual void OnError(int errorcode, WCHAR* errorMessage) = 0;
	virtual void OnTimeOut() = 0;

public:

	bool Disconnect();
	void IO_Cancel();

	//---------------------------------------------------
	// Send관련함수
	//---------------------------------------------------
	//bool SendPost();
	//bool SendPacket(NetPacket* packet);
	//void SendUnicast(NetPacket* packet);


};