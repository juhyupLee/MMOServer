#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용은 Windows 헤더에서 제외합니다.
#define NOMINMAX
//Windows 헤더 파일:


//표준
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <ostream>

//스레드관련

//국가 관련
#include <locale>

//STL 컨테이너
#include <map>
#include <unordered_map>
#include <vector>
#include <queue>
#include <set>
#include <unordered_set>

//문자열관련

#include <tchar.h>

#include <string>
#include <atlstr.h>

//기타등등
#include <assert.h>
#include <limits.h>
#include <timeapi.h>
#include <WS2tcpip.h>

#include <time.h>
#include <strsafe.h>


//네트워크 OS 관련
#include <process.h>
#include <WinSock2.h>
#include <Mswsock.h>
#include <Windows.h>
#include <thread>
#include <fstream>
#include <chrono>

#include <psapi.h>
#include <Dbghelp.h>

//개인라이브러리
#include "../Memory/Global.h"

#include "../Dump/MemoryDump.h"

#include "../Memory/LockFreeQ.h"
#include "../Memory/LockFreeStack.h"
#include "../Memory/RingBuffer.h"
#include "../Memory/Protocol.h"
#include "../Memory/MemoryPool_TLS.h"
#include "../Memory/SerializeBuffer.h"

// TODO: 프로그램에 필요한 추가 헤더는 여기에서 참조합니다.


