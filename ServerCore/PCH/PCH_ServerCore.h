#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용은 Windows 헤더에서 제외합니다.
#define NOMINMAX

//Windows 헤더 파일:
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "../bin/pqxx.lib")
#pragma comment(lib, "../bin/libpq.lib")
//국가 관련
#include <locale>
#include <stdint.h>
//표준
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <ostream>
#include <conio.h>
#include <memory>
#include <cstdint>
#include <clocale>
//스레드관련
#include <mutex>


//STL 컨테이너
#include <map>
#include <unordered_map>
#include <vector>
#include <queue>
#include <set>
#include <unordered_set>
#include <stack>
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
#include <regex>

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

#include <spdlog/spdlog.h>
//개인라이브러리
#include "../Utill/Singleton.h"
#include "../Utill/TimeUtill.h"
#include "../Utill/LogManager.h"
#include "../Utill/PointerStack.h"
#include "../Utill/UIDGenerator.h"
#include "../../flatbuffers/ProtocoID.h"
#include "../Memory/Global.h"

#include "../Dump/MemoryDump.h"

#include "../Memory/FreeList.h"
#include "../Memory/MemoryPool.h"
#include "../Memory/LockFreeQ.h"
#include "../Memory/LockFreeStack.h"
#include "../Memory/RingBuffer.h"
#include "../Memory/Protocol.h"
#include "../Memory/Chunk.h"
#include "../Memory/ObjectPool.h"


#include "../Network/Option.h"
#include "../Network/TemplateQ.h"
#include "../Network/JobQueue.h"
#include "../Network/JobDispatcher.h"
#include "../Network/NetworkSession.h"
#include "../Network/NetworkServer.h"
#include "../Network/NetworkTask.h"
#include "../Network/BaseServerApp.h"
#include "../Test/Test_MemoryPool.h"

// TODO: 프로그램에 필요한 추가 헤더는 여기에서 참조합니다.


