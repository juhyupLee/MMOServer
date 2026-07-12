#pragma once

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif

	#include <WinSock2.h>
	#include <WS2tcpip.h>
	#include <Windows.h>
	#include <Dbghelp.h>
	#include <Mswsock.h>
	#include <atlstr.h>
	#include <conio.h>
	#include <process.h>
	#include <psapi.h>
	#include <strsafe.h>
	#include <tchar.h>
	#include <timeapi.h>

	#ifndef MMO_CMAKE_BUILD
		#pragma comment(lib, "Ws2_32.lib")
		#pragma comment(lib, "Dbghelp.lib")
	#endif
#endif

#include <algorithm>
#include <array>
#include <assert.h>
#include <atomic>
#include <chrono>
#include <clocale>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <ostream>
#include <queue>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

#include "../../flatbuffers/ProtocoID.h"

#include "../Utill/Singleton.h"
#include "../Utill/TimeUtil.h"
#include "../Utill/LogManager.h"
#include "../Utill/PointerStack.h"
#include "../Utill/UIDGenerator.h"

#include "../Memory/Global.h"
#include "../Memory/RingBuffer.h"
#include "../Memory/Protocol.h"

#ifdef _WIN32
	// The custom allocator currently relies on Windows-specific 128-bit CAS
	// and address-layout assumptions. Keep it available to the existing
	// Windows experiments, but do not put it on the Linux server path.
	#include "../Memory/FreeList.h"
	#include "../Memory/MemoryPool.h"
	#include "../Memory/LockFreeQ.h"
	#include "../Memory/LockFreeStack.h"
	#include "../Memory/Chunk.h"
	#include "../Memory/ObjectPool.h"
	#include "../Test/Test_MemoryPool.h"
#endif

#include "../Dump/MemoryDump.h"

#include "../Network/Option.h"
#include "../Network/JobQueue.h"
#include "../Network/JobDispatcher.h"
#include "../Network/FrameDecoder.h"
#include "../Network/NetworkServer.h"
#include "../Network/NetworkSession.h"
#include "../Network/BaseServerApp.h"
