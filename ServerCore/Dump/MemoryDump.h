#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <DbgHelp.h>
#include <Psapi.h>
#include <crtdbg.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new.h>

#pragma comment(lib,"Dbghelp.lib")

void CRASH();

class CrashDump
{
public:
	CrashDump()
	{
		_invalid_parameter_handler oldHandler, newHandler;
		newHandler = myInvalidParameterHandler;

		oldHandler = _set_invalid_parameter_handler(newHandler);
		_CrtSetReportMode(_CRT_WARN, 0);
		_CrtSetReportMode(_CRT_ASSERT, 0);
		_CrtSetReportMode(_CRT_ERROR, 0);

		_CrtSetReportHook(_custom_Report_hook);

		//---------------------------------------------------------------------------------------------
		// pure virtual function called 에러 핸들러를 사용자 정의함수로 우회시킨다.
		//---------------------------------------------------------------------------------------------
		_set_purecall_handler(myPurecallHandler);

		SetHandlerDump();

	}

	static LONG WINAPI MyExceptionFilter(PEXCEPTION_POINTERS exceptionPointer)
	{
		int workingMemory = 0;
		SYSTEMTIME nowTime;
		long dumpCount = InterlockedIncrement(&m_DumpCount);

		//----------------------------------------------------------------------
		// 현재 프로세스의 메모리 사용량을 얻어온다
		//----------------------------------------------------------------------
		HANDLE process = 0;
		PROCESS_MEMORY_COUNTERS pmc;

		process = GetCurrentProcess();

		if (NULL == process)
		{
			return 0;
		}

		if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc)))
		{
			workingMemory = (int)(pmc.WorkingSetSize / 1024 / 1024);
		}
		//----------------------------------------------------------------------
		// 현재 날짜와 시간을 알아 온다
		//----------------------------------------------------------------------
		WCHAR fileName[MAX_PATH];
		GetLocalTime(&nowTime);

		wsprintf(fileName, L"Dump_%d%02d%02d_%02d.%02d.%02d_%d_%dMB.dmp",
			nowTime.wYear, nowTime.wMonth, nowTime.wDay, nowTime.wHour, nowTime.wMinute, nowTime.wSecond, dumpCount, workingMemory);
		wprintf(L"\n\n\n!!! Crash Error!!! %d.%d.%d / %d:%d:%d\n",
			nowTime.wYear, nowTime.wMonth, nowTime.wDay, nowTime.wHour, nowTime.wMinute, nowTime.wSecond);

		wprintf(L"Now Save Dump File....\n");

		HANDLE dumpFile = CreateFile(fileName,
									GENERIC_WRITE,
									FILE_SHARE_WRITE,
									NULL,
									CREATE_ALWAYS,
									FILE_ATTRIBUTE_NORMAL, NULL);

		if (dumpFile != INVALID_HANDLE_VALUE)
		{
			_MINIDUMP_EXCEPTION_INFORMATION miniDumpExceptionInformation;

			miniDumpExceptionInformation.ThreadId = GetCurrentThreadId();
			miniDumpExceptionInformation.ExceptionPointers = exceptionPointer;
			miniDumpExceptionInformation.ClientPointers = FALSE;

			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, MiniDumpWithFullMemory, &miniDumpExceptionInformation, NULL, NULL);

			CloseHandle(dumpFile);
			wprintf(L"CrashDump Save Finish!");
		}

		return EXCEPTION_EXECUTE_HANDLER;


	}

	static void SetHandlerDump()
	{
		SetUnhandledExceptionFilter(MyExceptionFilter);

	}
	static void myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t reserved)
	{
		CRASH();
	}

	static int _custom_Report_hook(int repostType, char* message, int* returnValue)
	{
		CRASH();
		return true;
	}
	static void myPurecallHandler()
	{
		CRASH();
	}


private:
	static long m_DumpCount;

};

#else

#include <csignal>
#include <cstdio>
#include <execinfo.h>
#include <unistd.h>

class CrashDump
{
public:
	CrashDump()
	{
		SetHandlerDump();
	}

private:
	static void SignalHandler(int signalNumber)
	{
		std::fprintf(stderr, "\n!!! Crash Error!!! signal=%d\n", signalNumber);
		void* callstack[128]{};
		const int frameCount = backtrace(callstack, 128);
		backtrace_symbols_fd(callstack, frameCount, STDERR_FILENO);

		std::signal(signalNumber, SIG_DFL);
		std::raise(signalNumber);
	}

	static void SetHandlerDump()
	{
		std::signal(SIGSEGV, SignalHandler);
		std::signal(SIGABRT, SignalHandler);
		std::signal(SIGFPE, SignalHandler);
		std::signal(SIGILL, SignalHandler);
		std::signal(SIGBUS, SignalHandler);
	}
};

#endif
