#pragma once
#ifndef MMO_CMAKE_BUILD
	#ifdef _DEBUG
	#	pragma comment (lib, "../bin/ServerCore_Debug.lib")
	#else
	#	pragma comment (lib, "../bin/ServerCore_Release.lib")
	#endif
#endif

#include "../../ServerCore/PCH/PCH_ServerCore.h"
#include "../PlayServer.h"
