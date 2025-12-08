#pragma once
#include <Windows.h>
#include <iostream>

class MyMMOServer;


enum eSendFlag
{
	SENDPOST_ENTRY,  //0
	BEFORE_SEND,     //1
	AFTER_SEND,      //2
	GQCS_SENDFLAG,   //3 
	SEND_COMPLETE_BEFORE, //4
	SEND_COMPLETE_AFTER,  //5
	RECVPOST,              //6,
	SIZE_SHORT, //7
	YIELD, //8
	IO_PENDING //9
};

enum eIOCP_LINE
{
	GQCS,
	ACCEPT_BEFORE_RECV,
	ACCEPT_NEW_USER,
	ACCEPT_AFTER_RECV,
	RECVPOST_BEFORE_RECV,
	RECVPOST_AFTER_RECV,
	SENDPOST_BEFORE_SEND,
	SENDPACKET,
	SENDPOST_AFTER_SEND,
	RECV_COMPLETE,
	SEND_COMPLETE,
	LAST_COMPLETE,
	TRANSFER_ZERO_RECV,
	TRANSFER_ZERO_SEND,
	CLOSE_SOCKET_ENTRY,
	RELEASE_SESSION,
	RELASE_SESSION_CLEAR_IO_INC,
	RECVPOST_RECVRTN,
	SENDPOST_SENDRTN,
	ACCEPT_RECVRTN,
	ELSE_OVERLAP,
	CLOSE_SOCKET_INVALIDSOCKET,
	CLOSE_SOCKET_CLOSESOCKET,
	SENDCOMPLETE_BEFROE_GETUSEDSIZE,
	SENDCOMPLETE_AFTER_GETUSEDSIZE,
	CRASH,
	ON_RECV,
	ON_RECV_ALLOC,
	ON_RECV_FREE,
	ON_RECV_LAST,
	DEQPACKET_ENTRY,
	DEQPACKET_LAST,
	DEQPACKET_FREE,
	RELEASEPACKET_FREE,
	ON_CLIENTJOIN_ALLOC,
	ON_CLIENTJOIN_LAST,
	WSABUF_SETTING,
	WSABUF_SETTING_LAST,
	SMARTPACKET_COPY_STRUCTOR,
	SMARTPACKET_EXCHANGE,
	SMARTPACKET_DESTRUCTOR,
	SMARTPACKET_DELETE,

	UPDATETHREAD_JOB_SWITCH,
	MESSAGE_MARSHAL,
	MESSAGE,
	ACQUIRE_SESSION_SUC,
	ACQUIRE_SESSION_FAIL,
	ON_CLIENT_LEAVE,
	ON_CLIENT_JOIN,
	CREATE_NEW_CLIENT,
	DELETE_CLIENT,
	DISCONNECT_CLIENT,
	RECVPOST_FREESIZE_NON,
	PQCS_SENDPOST,

	DISCON_RELEASE,

	SENDTHREAD_SENDFAIL,
	SENDTHREAD_SENDSUC


};


struct SendFlag_Log
{
	int64_t _No;
	eSendFlag _Pos;
	DWORD _ThreadID;
	SOCKET _Socket;
	int64_t _CurSession;
	int64_t _RecvOL;
	int64_t _SendOL;
	LONG _SendFlag;
	int32_t _SendQUsedSize;

	void DataSettiong(int64_t no, eSendFlag pos, DWORD threadid, int64_t socket, int64_t curSession, int64_t  recvOL, int64_t  sendOL, LONG snedFlag,int32_t sendQUsedSize)
	{
		_No = no;
		_Pos = pos;
		_ThreadID = threadid;
		_Socket = socket;
		_CurSession = curSession;
		_RecvOL = recvOL;
		_SendOL = sendOL;
		_SendFlag = snedFlag;
		_SendQUsedSize = sendQUsedSize;
	}
};

enum eRecvMessageType
{
	NOTHING=0,
	LOGIN=1,
	SECTOR_MOVE = 3,
	MESSAGE_RECV=5
	

};
struct IOCP_Log
{
	int64_t _No;
	eIOCP_LINE _Pos;
	DWORD _ThreadID;
	SOCKET _Socket;
	DWORD _IOCount;
	int64_t _CurSession;
	uint64_t _CurSessionID;
	int64_t _RecvOL;
	int64_t _SendOL;
	LONG _SendFlag;
	int32_t _JobType;
	int32_t _JobQCount;
	eRecvMessageType _RecvMessageType;
	int32_t _PacketRefCount;
	int64_t _AccountNo;
	int32_t _SessionStatus;



	void DataSettiong(int64_t no, eIOCP_LINE pos, DWORD threadid, int64_t socket, DWORD ioCount, int64_t curSession, uint64_t curSessionID, int64_t  recvOL, int64_t  sendOL, LONG snedFlag, int32_t jobType=-1, int32_t jobQCount=-1, eRecvMessageType type= eRecvMessageType::NOTHING, int32_t refCount=-1,int64_t accountNo =-1, int32_t sessionStatus=-1)
	{
		_No = no;
		_Pos = pos;
		_ThreadID = threadid;
		_Socket = socket;
		_IOCount = ioCount;
		_CurSession = curSession;
		_CurSessionID = curSessionID;
		_RecvOL = recvOL;
		_SendOL = sendOL;
		_SendFlag = snedFlag;
		_JobType = jobType;
		_JobQCount = jobQCount;
		_RecvMessageType = type;
		_PacketRefCount = refCount;
		_AccountNo = accountNo;
		_SessionStatus = sessionStatus;
	}

};

enum class eFreeListPos
{
	DEQ_POINTER = 1, //1
	ENQ_POINTER,   //2
	FREE_FLAG_CHECK, //3
	FREE_OKAY,  //4
	ALLOC_OKAY,//5
	ALLOC_MEMORY//6
};
struct FreeList_Log
{
	int64_t _No;
	eFreeListPos _POS;
	DWORD _ThreadID;
	int64_t _TopPtr;
	int64_t _TempTopPtr;
	int64_t _FreeNode;
	int64_t _ReturnNode;


	void DataSettiong(int64_t no, eFreeListPos pos, DWORD threadid, int64_t _TopPtr, int64_t _TempTopPtr,int64_t freeNode,int64_t  _returnNode)
	{
		_No = no;
		_POS = pos;
		_ThreadID = threadid;
		_TopPtr = _TopPtr;
		_TempTopPtr = _TempTopPtr;
		_FreeNode = freeNode;
		_ReturnNode = _returnNode;
	}
};
enum class ePOS
{
	ENTRY_ENQ,
	ALLOC_NODE_ENQ,
	TEMP_REAR_SETTING_ENQ,
	CHANGEVALUE_SETTING_ENQ,
	CAS1_FAIL_BUT_CHANGE_REAR_ENQ,
	CAS1_FAIL_NO_CHANGE,
	CAS1_SUC_ENQ,
	CAS2_SUC_NEWNODE1_ENQ,
	CAS2_FAIL_NEWNODE1_ENQ,
	CAS2_SUC_NEWNODE2_ENQ,
	CAS2_FAIL_NEWNODE2_ENQ,
	LOOP_OUT_ENQ,

	ENTRY_DEQ,
	NODE_ZERO_DEQ1,
	NODE_ZERO_DEQ2,
	TEMP_FRONT_SETTING_DEQ,
	CHANGEVALUE_SETTING_DEQ,
	CAS2_SUC_DEQ,
	CAS2_FAIL_DEQ,
	LOOP_OUT_DEQ,
	FREE_NODE_DEQ,
	RETURN_DATA_DEQ,
	TEMPREAR_NEWNODE_SAME,
	NEWNODE_NEXT_NOT_NULL


};
struct Q_LOG
{
	int64_t _No;
	ePOS _POS;
	DWORD _ThreadID;
	int64_t _FrontPtr;
	int64_t _RearPtr;
	int64_t _TempFrontPtr;
	int64_t _TempRearPtr;
	int64_t _NewNodePtr;
	int64_t _Data;
	int64_t _RearNext;
	int64_t _CurDummyNode;
	int64_t _NewNext;
	LONG _Count;
	int32_t _LoopCount;

	void DataSettiong(int64_t no, ePOS pos, DWORD threadid, int64_t frontPtr, int64_t rearPtr, int64_t _tempFrontPtr, int64_t tempRearPtr, int64_t newNodePtr, int64_t data, int64_t rearNext, int64_t curDummy,int64_t newNext=-1, LONG count=-1,int loopCount=-1)
	{
		_No = no;
		_POS = pos;
		_ThreadID = threadid;
		_FrontPtr = frontPtr;
		_RearPtr = rearPtr;
		_TempFrontPtr = _tempFrontPtr;
		_TempRearPtr = tempRearPtr;
		_NewNodePtr = newNodePtr;
		_Data = data;
		_RearNext = rearNext;
		_CurDummyNode = curDummy;
		_NewNext = newNext;
		_Count = count;
		_LoopCount = loopCount;
	}
};

template <typename T,size_t size = 5000 >
class MemoryLogging_New
{
public:
	enum
	{
		USE_LOG = 1,
		MAX_SIZE = size
	};
public:
	MemoryLogging_New()
		:m_Index(0),
		m_Count(0),
		m_Buffer{ 0, }
	{
	}

public:
	void MemoryLogging(T logData);
	void Clear();

private:
	T  m_Buffer[MAX_SIZE];
	uint64_t m_Index;
	uint64_t m_Count;
};
template <size_t size = 5000 >
class MemoryLogging_ST
{

public:
	enum
	{
		USE_LOG = 1,
		MAX_SIZE = size
	};
public:
	MemoryLogging_ST()
		:m_Index(0),
		m_Count(0),
		m_Buffer{0,}
	{
	}

public:
	void MemoryLogging(Q_LOG logData);
	void Clear();

private:
	Q_LOG  m_Buffer[MAX_SIZE];
	uint64_t m_Index;
	uint64_t m_Count;
};

template<size_t size>
inline void MemoryLogging_ST<size>::MemoryLogging(Q_LOG logData)
{
	if (!USE_LOG)
	{
		return;
	}
	uint64_t tempIndex = InterlockedIncrement(&m_Index) % MAX_SIZE;
	ZeroMemory(&m_Buffer[tempIndex],  sizeof(Q_LOG));
	m_Buffer[tempIndex] = logData;

}

template<typename T, size_t size>
inline void MemoryLogging_New<T, size>::MemoryLogging(T logData)
{
	if (!USE_LOG)
	{
		return;
	}
	uint64_t tempIndex = InterlockedIncrement(&m_Index) % MAX_SIZE;
	ZeroMemory(&m_Buffer[tempIndex], sizeof(T));
	m_Buffer[tempIndex] = logData;
}

template<typename T, size_t size>
inline void MemoryLogging_New<T, size>::Clear()
{
	memset(m_Buffer, 0, sizeof(T) * MAX_SIZE);
	m_Index = 0;
	m_Count = 0;
}

template<size_t size>
inline void MemoryLogging_ST<size>::Clear()
{
	memset(m_Buffer, 0, sizeof(Q_LOG) * MAX_SIZE);
	m_Index = 0;
	m_Count = 0;
}

//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------

template <typename T,size_t size=5000 >
class MyMemoryLog
{
public:
	enum
	{
		USE_LOG = 1,
		MAX_SIZE = size,
		MAX_PROPERTY = 10
	};
public:
	MyMemoryLog()
		:m_Index(0),
		 m_Count(0)
	{
		memset(m_Buffer, 0, sizeof(T) * MAX_SIZE * MAX_PROPERTY);
	}

public:
	void MemoryLogging(T pos, T threadID, T data);
	void MemoryLogging(T pos, T threadID, T sessionID, T data);
	void MemoryLogging(T pos, T threadID, T sessionID, T data1, T data2);
	void MemoryLogging(T pos, T threadID, T sessionID, T data1, T data2, T data3);
	void MemoryLogging(T pos, T threadID, T sessionID, T data1, T data2, T data3, T data4);
	void MemoryLogging(T pos, T threadID, T sessionID, T data1, T data2, T data3, T data4, T data5);
	void MemoryLogging(T pos, T threadID, T sessionID, T data1, T data2, T data3, T data4, T data5, T data6);

	void Clear();

private:
	T  m_Buffer[MAX_SIZE][MAX_PROPERTY];
	uint64_t m_Index;
	uint64_t m_Count;
};

template<typename T, size_t size>
inline void MyMemoryLog<T, size>::MemoryLogging(T pos, T threadID, T data)
{
	if (!USE_LOG)
	{
		return;
	}
	uint64_t tempIndex = InterlockedIncrement(&m_Index) % MAX_SIZE;
	ZeroMemory(m_Buffer[tempIndex], MAX_PROPERTY * sizeof(T));
	m_Buffer[tempIndex][0] = (T)InterlockedIncrement(&m_Count);
	m_Buffer[tempIndex][1] = threadID;
	m_Buffer[tempIndex][2] = pos;
	m_Buffer[tempIndex][3] = data;
}

template <typename T, size_t size >
inline void MyMemoryLog<T,size>::MemoryLogging(T pos, T threadID, T sessionID, T data)
{
	if (!USE_LOG)
	{
		return;
	}
	uint64_t tempIndex = InterlockedIncrement(&m_Index) % MAX_SIZE;
	ZeroMemory(m_Buffer[tempIndex], MAX_PROPERTY * sizeof(T));
	m_Buffer[tempIndex][0] = (T)InterlockedIncrement(&m_Count);
	m_Buffer[tempIndex][1] = sessionID;
	m_Buffer[tempIndex][2] = threadID;
	m_Buffer[tempIndex][3] = pos;
	m_Buffer[tempIndex][4] = data;
}

template <typename T, size_t size >
inline void MyMemoryLog<T,size>::MemoryLogging(T pos, T threadID, T sessionID, T data1, T data2)
{
	if (!USE_LOG)
	{
		return;
	}
	uint64_t tempIndex = InterlockedIncrement(&m_Index) % MAX_SIZE;
	ZeroMemory(m_Buffer[tempIndex], MAX_PROPERTY * sizeof(T));

	m_Buffer[tempIndex][0] = (T)InterlockedIncrement(&m_Count);
	m_Buffer[tempIndex][1] = sessionID;
	m_Buffer[tempIndex][2] = threadID;
	m_Buffer[tempIndex][3] = pos;
	m_Buffer[tempIndex][4] = data1;
	m_Buffer[tempIndex][5] = data2;
}

template <typename T, size_t size >
inline void MyMemoryLog<T,size>::MemoryLogging(T pos, T threadID, T sessionID, T data1, T data2, T data3)
{
	if (!USE_LOG)
	{
		return;
	}
	uint64_t tempIndex = InterlockedIncrement(&m_Index) % MAX_SIZE;

	ZeroMemory(m_Buffer[tempIndex], MAX_PROPERTY * sizeof(T));

	m_Buffer[tempIndex][0] = (T)InterlockedIncrement(&m_Count);
	m_Buffer[tempIndex][1] = sessionID;
	m_Buffer[tempIndex][2] = threadID;
	m_Buffer[tempIndex][3] = pos;
	m_Buffer[tempIndex][4] = data1;
	m_Buffer[tempIndex][5] = data2;
	m_Buffer[tempIndex][6] = data3;
}

template <typename T, size_t size >
inline void MyMemoryLog<T,size>::MemoryLogging(T pos, T threadID, T sessionID, T data1, T data2, T data3, T data4)
{
	if (!USE_LOG)
	{
		return;
	}
	uint64_t tempIndex = InterlockedIncrement(&m_Index) % MAX_SIZE;

	ZeroMemory(m_Buffer[tempIndex], MAX_PROPERTY * sizeof(T));

	m_Buffer[tempIndex][0] = (T)InterlockedIncrement(&m_Count);
	m_Buffer[tempIndex][1] = sessionID;
	m_Buffer[tempIndex][2] = threadID;
	m_Buffer[tempIndex][3] = pos;
	m_Buffer[tempIndex][4] = data1;
	m_Buffer[tempIndex][5] = data2;
	m_Buffer[tempIndex][6] = data3;
	m_Buffer[tempIndex][7] = data4;
}

template <typename T, size_t size >
inline void MyMemoryLog<T,size>::MemoryLogging(T pos, T threadID, T sessionID, T data1, T data2, T data3, T data4, T data5)
{
	if (!USE_LOG)
	{
		return;
	}
	uint64_t tempIndex = InterlockedIncrement(&m_Index) % MAX_SIZE;

	ZeroMemory(m_Buffer[tempIndex], MAX_PROPERTY * sizeof(T));

	m_Buffer[tempIndex][0] = (T)InterlockedIncrement(&m_Count);
	m_Buffer[tempIndex][1] = sessionID;
	m_Buffer[tempIndex][2] = threadID;
	m_Buffer[tempIndex][3] = pos;
	m_Buffer[tempIndex][4] = data1;
	m_Buffer[tempIndex][5] = data2;
	m_Buffer[tempIndex][6] = data3;
	m_Buffer[tempIndex][7] = data4;
	m_Buffer[tempIndex][8] = data5;
}

template <typename T, size_t size >
inline void MyMemoryLog<T,size>::MemoryLogging(T pos, T threadID, T sessionID, T data1, T data2, T data3, T data4, T data5, T data6)
{
	if (!USE_LOG)
	{
		return;
	}
	uint64_t tempIndex = InterlockedIncrement(&m_Index) % MAX_SIZE;

	ZeroMemory(m_Buffer[tempIndex], MAX_PROPERTY * sizeof(T));

	m_Buffer[tempIndex][0] = (T)InterlockedIncrement(&m_Count);
	m_Buffer[tempIndex][1] = sessionID;
	m_Buffer[tempIndex][2] = threadID;
	m_Buffer[tempIndex][3] = pos;
	m_Buffer[tempIndex][4] = data1;
	m_Buffer[tempIndex][5] = data2;
	m_Buffer[tempIndex][6] = data3;
	m_Buffer[tempIndex][7] = data4;
	m_Buffer[tempIndex][8] = data5;
	m_Buffer[tempIndex][9] = data6;
}

template<typename T, size_t size>
inline void MyMemoryLog<T, size>::Clear()
{
	memset(m_Buffer, 0, sizeof(T) * MAX_SIZE );	
	m_Index = 0;
	m_Count = 0;
}

