#include "Global.h"

LONG g_IOPostCount = 0;
LONG g_IOCompleteCount = 0;
LONG g_IOIncDecCount = 0;

MemoryLogging_New<IOCP_Log, 5000> g_MemoryLog_IOCP;
int64_t g_IOCPMemoryNo = -1;

Profiler g_Profiler;

LONG g_FreeMemoryCount = 0;
LONG g_AllocMemoryCount = 0;

LONG g_OnClientLeaveCall = 0;
LONG g_DisconnectCount = 0;
LONG g_TCPTimeoutReleaseCnt = 0;

void Crash()
{
	int* p = nullptr;
	*p = 20;
}