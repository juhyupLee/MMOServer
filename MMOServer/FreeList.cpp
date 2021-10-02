#include "FreeList.h"
long long g_Mark = 0x0000000000000001;
MemoryLogging_New<FreeList_Log, 10000> 
LogFreeList; 
int64_t g_FreeListMemoryCount =0; 