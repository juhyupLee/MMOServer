#pragma once

#define WIN32_LEAN_AND_MEAN             // ���� ������ �ʴ� ������ Windows ������� �����մϴ�.
#define NOMINMAX
//Windows ��� ����:


//ǥ��
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <ostream>

//���������

//���� ����
#include <locale>

//STL �����̳�
#include <map>
#include <unordered_map>
#include <vector>
#include <queue>
#include <set>
#include <unordered_set>

//���ڿ�����

#include <tchar.h>

#include <string>
#include <atlstr.h>

//��Ÿ���
#include <assert.h>
#include <limits.h>
#include <timeapi.h>
#include <WS2tcpip.h>

#include <time.h>
#include <strsafe.h>


//��Ʈ��ũ OS ����
#include <process.h>
#include <WinSock2.h>
#include <Mswsock.h>
#include <Windows.h>
#include <thread>
#include <fstream>
#include <chrono>

#include <psapi.h>
#include <Dbghelp.h>

//���ζ��̺귯��
#include "../Memory/Global.h"

#include "../Dump/MemoryDump.h"

#include "../Memory/LockFreeQ.h"
#include "../Memory/LockFreeStack.h"
#include "../Memory/RingBuffer.h"
#include "../Memory/Protocol.h"
#include "../Memory/MemoryPool_TLS.h"
#include "../Memory/SerializeBuffer.h"

// TODO: ���α׷��� �ʿ��� �߰� ����� ���⿡�� �����մϴ�.


