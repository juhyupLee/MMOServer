#pragma once

// ============================================================
// Platform Abstraction (must be first)
// ============================================================
#include "../Platform/PlatformDef.h"

// ============================================================
// Windows-specific headers (guarded)
// ============================================================
#ifdef _WIN32
	#pragma comment(lib, "Ws2_32.lib")
	#pragma comment(lib, "Mswsock.lib")
	#pragma comment(lib, "../bin/libpq.lib")
	#pragma comment(lib, "../bin/pqxx.lib")
	#include <conio.h>
	#include <tchar.h>
	#include <atlstr.h>
	#include <timeapi.h>
	#include <process.h>
	#include <WinSock2.h>
	#include <Mswsock.h>
	#include <WS2tcpip.h>
	#include <psapi.h>
	#include <Dbghelp.h>
	#include <strsafe.h>
#endif

// ============================================================
// Standard C/C++ Headers
// ============================================================
#include <locale>
#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <ostream>
#include <memory>
#include <cstdint>
#include <clocale>
#include <cstring>

// Thread
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <functional>

// STL Containers
#include <map>
#include <unordered_map>
#include <vector>
#include <queue>
#include <deque>
#include <set>
#include <unordered_set>
#include <stack>
#include <variant>
#include <optional>
#include <ranges>
#include <algorithm>

// String
#include <string>

// Misc
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <regex>
#include <concepts>
#include <new>

// ============================================================
// Third-party Libraries
// ============================================================
#include <pqxx/pqxx>
#include <spdlog/spdlog.h>
#include <boost/asio.hpp>

// ============================================================
// Project Headers
// ============================================================
#include "../Utill/Singleton.h"
#include "../Utill/TimeUtil.h"
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

#include "../DB/DBSession.h"
