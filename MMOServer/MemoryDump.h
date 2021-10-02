#pragma once
#pragma comment(lib,"Dbghelp.lib")
#include <iostream>
#include <Windows.h>
#include <psapi.h>
#include <Dbghelp.h>

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

	static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS exceptionPointer)
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
		CloseHandle(process);

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
			miniDumpExceptionInformation.ClientPointers = TRUE;

			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, MiniDumpWithFullMemory, &miniDumpExceptionInformation, NULL, NULL);
			
			CloseHandle(dumpFile);
			wprintf(L"CrashDump Save Finish!");
		}

		return EXCEPTION_EXECUTE_HANDLER;


	}
	static void Crash()
	{
		int* p = nullptr;
		*p = 10;
	}

	static void SetHandlerDump()
	{
		SetUnhandledExceptionFilter(MyExceptionFilter);

	}
	static void myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t reserved)
	{
		Crash();
	}

	static int _custom_Report_hook(int repostType, char* message, int* returnValue)
	{
		Crash();
		return true;
	}
	static void myPurecallHandler()
	{
		Crash();
	}


private:
	static long m_DumpCount;

};
