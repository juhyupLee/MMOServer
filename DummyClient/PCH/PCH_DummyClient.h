#pragma once
#ifdef _DEBUG
#	pragma comment (lib, "../bin/ServerCore_Debug.lib")
#else
#	pragma comment (lib, "../bin/ServerCore_Release.lib")
#endif

#include "../../ServerCore/PCH/PCH_ServerCore.h"
#include "../DummyClient.h"
