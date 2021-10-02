#pragma once
#include <iostream>
#include <Windows.h>
#include "MemoryLog.h"
#include "Profiler.h"

#define dfPACKET_CODE		0x77
#define dfPACKET_KEY		0x32

#define dfSECTOR_X_MAX		50
#define dfSECTOR_Y_MAX		50

extern MemoryLogging_New<IOCP_Log, 5000> g_MemoryLog_IOCP;
extern int64_t g_IOCPMemoryNo;
extern LONG g_IOPostCount;
extern LONG g_IOCompleteCount;
extern LONG g_IOIncDecCount;
extern Profiler g_Profiler;

extern LONG g_FreeMemoryCount;
extern LONG g_AllocMemoryCount;


extern LONG g_OnClientLeaveCall;
extern LONG g_DisconnectCount;
extern LONG g_TCPTimeoutReleaseCnt;


class MyLock
{
public:
	MyLock()
	{
		InitializeSRWLock(&m_Lock);
	}
	void Lock()
	{
		AcquireSRWLockExclusive(&m_Lock);
	}
	void Unlock()
	{
		ReleaseSRWLockExclusive(&m_Lock);
	}
private:
	SRWLOCK m_Lock;
};

void Crash();