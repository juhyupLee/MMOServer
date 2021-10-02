#include "Log.h"
#include <time.h>
#include <strsafe.h>


SysLog::SysLog()
	:m_ServerONLogFileName{0,}
	,m_LogLevel(eLogLevel::LOG_LEVEL_DEBUG)
	,m_DirectoryName{0,}
{

	InitializeSRWLock(&m_Lock);
	m_LogCount = 0;

	//-------------------------------------------------------------
	// 	   서버가 켜짐과동시에 로그 파일 생성
	//-------------------------------------------------------------
#if SERVER_ON_LOG ==1
	time_t curTime;
	tm today;
	FILE* filePtr = nullptr;

	time(&curTime);
	localtime_s(&today, &curTime);
		
	wsprintf(m_ServerONLogFileName, L"ServerLog[20%d%02d%02d -%d[h]%d[m]%d[s]].txt", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec);
	_wfopen_s(&filePtr, m_ServerONLogFileName, L"wt,ccs=UTF-16LE");
	while (filePtr == nullptr)
	{
		_wfopen_s(&filePtr, m_ServerONLogFileName, L"wt,ccs=UTF-16LE");
	}
	fclose(filePtr);
	
#endif

}

SysLog::~SysLog()
{
	
}

SysLog* SysLog::GetInstance()
{
	static SysLog log;

	return &log;
}

void SysLog::WriteLog(const WCHAR* type, eLogLevel logLevel, const WCHAR* stringFormat, ...)
{
	if (m_LogLevel > logLevel)
	{
		return;
	}
	time_t curTime;
	tm today;
	FILE* filePtr = nullptr;
	va_list va;
	va_start(va, stringFormat);
	WCHAR message[BUFFER_SIZE] = { 0, };

	//--------------------------------------------------------
	// 	   현재 기점으로 시간을 구한다.
	//--------------------------------------------------------
	time(&curTime);
	localtime_s(&today, &curTime);

	//-------------------------------------------------
	// 	   락을 검.
	//-------------------------------------------------
	AcquireSRWLockExclusive(&m_Lock);
	//-------------------------------------------------
#if SERVER_ON_LOG== 1
	//--------------------------------------------------------
	// 	   로그 실패시, 로그에 대한 로그를 남기는곳.
	//--------------------------------------------------------
	if (type == nullptr)
	{
		WCHAR fileName[MAX_PATH] = { 0, };
		HRESULT resultFile = StringCchPrintf(fileName, MAX_PATH, L"%s[20%d0%d].txt", L"LogFail", today.tm_year - 100, today.tm_mon + 1);

		//--------------------------------------------------------------
		// 	   File Open
		//--------------------------------------------------------------
		_wfopen_s(&filePtr, fileName, L"at,ccs=UTF-16LE");
		while (filePtr == nullptr)
		{
			_wfopen_s(&filePtr, fileName, L"at,ccs=UTF-16LE");
		}

		//--------------------------------------------------------------
		// 	  Safe String으로 문자열 복사 
		//--------------------------------------------------------------
		HRESULT result = StringCchVPrintf(message, BUFFER_SIZE, stringFormat, va);

		if (result == STRSAFE_E_INVALID_PARAMETER)
		{
		}
		else if (result == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			//-------------------------------------------------------
			// 내가 준비한 버퍼 부족으로 문자열이 잘렸을 경우, 이 잘림에 대한 로그를 남긴다.
			// 그리고 다음번 패치때 수정한다.
			//-------------------------------------------------------
			WCHAR mustSucMessage[BUFFER_SIZE] = { 0, };
			StringCchVPrintf(mustSucMessage, 255, L"문자열 버퍼 부족으로 문자열이 잘렸습니다↓", va);
			fwprintf(filePtr, L"[%d/%d/%d %d:%d:%d][%s//%s] %s\n", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, L"LogFail", L"ERROR", message);
		}

		switch (logLevel)
		{
		case eLogLevel::LOG_LEVEL_DEBUG:
			fwprintf(filePtr, L"[%d/%d/%d %d:%d:%d][%s//%s] %s\n", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, L"LogFail", L"DEBUG", message);
			break;
		case eLogLevel::LOG_LEVEL_ERROR:
			fwprintf(filePtr, L"[%d/%d/%d %d:%d:%d][%s//%s] %s\n", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, L"LogFail", L"ERROR", message);
			break;
		case eLogLevel::LOG_LEVEL_SYSTEM:
			fwprintf(filePtr, L"[%d/%d/%d %d:%d:%d][%s//%s] %s\n", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, L"LogFail", L"SYSTEM", message);
			break;
		}
		fclose(filePtr);
	}
	else
	{
		//--------------------------------------------------------------
		// 	   File Open
		//--------------------------------------------------------------
		_wfopen_s(&filePtr, m_ServerONLogFileName, L"at,ccs=UTF-16LE");
		while (filePtr == nullptr)
		{
			_wfopen_s(&filePtr, m_ServerONLogFileName, L"at,ccs=UTF-16LE");
		}

		//--------------------------------------------------------------
		// 	  Safe String으로 문자열 복사 
		//--------------------------------------------------------------
		HRESULT result = StringCchVPrintf(message, BUFFER_SIZE, stringFormat, va);

		if (result == STRSAFE_E_INVALID_PARAMETER)
		{
		}
		else if (result == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			//-------------------------------------------------------
			// 내가 준비한 버퍼 부족으로 문자열이 잘렸을 경우, 이 잘림에 대한 로그를 남긴다.
			// 그리고 다음번 패치때 수정한다.
			//-------------------------------------------------------
			WCHAR mustSucMessage[BUFFER_SIZE] = { 0, };
			StringCchVPrintf(mustSucMessage, 255, L"문자열 버퍼 부족으로 문자열이 잘렸습니다↓", va);
			fwprintf(filePtr, L"[%d/%d/%d %d:%d:%d][%s//%s] %s\n", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, L"LogFail", L"ERROR", message);
		}

		switch (logLevel)
		{
		case eLogLevel::LOG_LEVEL_DEBUG:
			fwprintf(filePtr, L"[%d/%d/%d %d:%d:%d][%s//%s//%010lld] %s\n", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, type, L"DEBUG", ++m_LogCount, message);
			break;
		case eLogLevel::LOG_LEVEL_ERROR:
			fwprintf(filePtr, L"[%d/%d/%d %d:%d:%d][%s//%s//%010lld] %s\n", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, type, L"ERROR", ++m_LogCount, message);
			break;
		case eLogLevel::LOG_LEVEL_SYSTEM:
			fwprintf(filePtr, L"[%d/%d/%d %d:%d:%d][%s//%s//%010lld] %s\n", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, type, L"SYSTEM", ++m_LogCount, message);
			break;
		}

		fclose(filePtr);
		
	}
#endif 
#if SERVER_ON_LOG== 0
	//--------------------------------------------------------
	// 	   로그 실패시, 로그에 대한 로그를 남기는곳.
	//--------------------------------------------------------
	if (type == nullptr)
	{
		WCHAR fileName[MAX_PATH] = { 0, };
		HRESULT resultFile = StringCchPrintf(fileName, MAX_PATH, L"%s[20%d%02d].txt",L"LogFail", today.tm_year - 100, today.tm_mon + 1);

		//--------------------------------------------------------------
		// 	   File Open
		//--------------------------------------------------------------
		_wfopen_s(&filePtr, fileName, L"at,ccs=UTF-16LE");
		while (filePtr == nullptr)
		{
			_wfopen_s(&filePtr, fileName, L"at,ccs=UTF-16LE");
		}

		//--------------------------------------------------------------
		// 	  Safe String으로 문자열 복사 
		//--------------------------------------------------------------
		HRESULT result = StringCchVPrintf(message, BUFFER_SIZE, stringFormat, va);

		if (result == STRSAFE_E_INVALID_PARAMETER)
		{
		}
		else if (result == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			//-------------------------------------------------------
			// 내가 준비한 버퍼 부족으로 문자열이 잘렸을 경우, 이 잘림에 대한 로그를 남긴다.
			// 그리고 다음번 패치때 수정한다.
			//-------------------------------------------------------
			WCHAR mustSucMessage[BUFFER_SIZE] = { 0, };
			StringCchVPrintf(mustSucMessage, 255, L"문자열 버퍼 부족으로 문자열이 잘렸습니다↓", va);
			fwprintf(filePtr, L"[%s][%d/%d/%d %d:%d:%d][%s] %s\n", L"LogFail", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, L"ERROR", mustSucMessage);
		}

		switch (logLevel)
		{
		case eLogLevel::LOG_LEVEL_DEBUG:
			fwprintf(filePtr, L"[%s][%d/%d/%d %d:%d:%d][%s] %s\n", L"LogFail", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, L"DEBUG", message);
			break;
		case eLogLevel::LOG_LEVEL_ERROR:
			fwprintf(filePtr, L"[%s][%d/%d/%d %d:%d:%d][%s] %s\n", L"LogFail", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, L"ERROR", message);
			break;
		case eLogLevel::LOG_LEVEL_SYSTEM:
			fwprintf(filePtr, L"[%s][%d/%d/%d %d:%d:%d][%s] %s\n", L"LogFail", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, L"SYSTEM", message);
			break;
		}
		fclose(filePtr);
	}
	//--------------------------------------------------------
	// 	   일반적인 시스템 로그를 남기는곳.
	//--------------------------------------------------------
	else
	{
		//--------------------------------------------------------
		// 	  type이 있는 로그파일을 검색한다 없으면 새로추가한다.
		//--------------------------------------------------------
		auto iter = m_FileNameMap.find(type);
		if (iter == m_FileNameMap.end())
		{
			WCHAR fileName[MAX_PATH] = { 0, };

			//-------------------------------------------------------
			// MAX_PATH 이므로 실패하면 안됨.
			//-------------------------------------------------------
			HRESULT result = StringCchPrintf(fileName, MAX_PATH, L"%s%s[20%d%02d].txt", m_DirectoryName, type, today.tm_year - 100, today.tm_mon + 1);

			LogFileName* logfileNamePtr = new LogFileName;

			logfileNamePtr->_FileName = fileName;
			logfileNamePtr->_LastMonth = today.tm_mon + 1;

		
			m_FileNameMap.insert(std::make_pair(type, logfileNamePtr));

			iter = m_FileNameMap.find(type);
		}

		LogFileName* tempLogFileNamePtr = (*iter).second;

		//--------------------------------------------------------------
		// 	   월(Month)이 변경되면 LastMonth를 갱신해주고 파일 이름을 새로 세팅해준뒤,
		//     파일 오픈을해야된다.
		//--------------------------------------------------------------
		if (tempLogFileNamePtr->_LastMonth != today.tm_mon + 1)
		{
			WCHAR fileName[MAX_PATH] = { 0, };

			//-------------------------------------------------------
			// MAX_PATH 이므로 실패하면 안됨.
			//-------------------------------------------------------
			HRESULT result = StringCchPrintf(fileName, MAX_PATH, L"%s%s[20%d%02d].txt", m_DirectoryName, type, today.tm_year - 100, today.tm_mon + 1);
			tempLogFileNamePtr->_FileName = fileName;
			tempLogFileNamePtr->_LastMonth = today.tm_mon + 1;
		}

		//--------------------------------------------------------------
		// 	   File Open
		//--------------------------------------------------------------
		const WCHAR* cstringFileName = tempLogFileNamePtr->_FileName.c_str();


		_wfopen_s(&filePtr, cstringFileName, L"at,ccs=UTF-16LE");
		while (filePtr == nullptr)
		{
			_wfopen_s(&filePtr, cstringFileName, L"at,ccs=UTF-16LE");
		}

		//--------------------------------------------------------------
		// 	  로그내용을 Safe String으로 문자열 복사 
		//--------------------------------------------------------------
		HRESULT result = StringCchVPrintf(message, BUFFER_SIZE, stringFormat, va);

		if (result == STRSAFE_E_INVALID_PARAMETER)
		{
		}
		else if (result == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			//-------------------------------------------------------
			// 내가 준비한 버퍼 부족으로 문자열이 잘렸을 경우, 이 잘림에 대한 로그를 남긴다.
			// 그리고 다음번 패치때 수정한다.
			//-------------------------------------------------------
			WCHAR mustSucMessage[BUFFER_SIZE] = { 0, };
			StringCchVPrintf(mustSucMessage, 255, L"문자열 버퍼 부족으로 문자열이 잘렸습니다↓", va);
			fwprintf(filePtr, L"[%s][%d/%d/%d %d:%d:%d][%s] %s\n", type, today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, L"ERROR", mustSucMessage);

		}

		//--------------------------------------------------------------
		// 	  실제 로그 내용을, File에 한줄씩 fwprintf한다.
		//--------------------------------------------------------------
		switch (logLevel)
		{
		case eLogLevel::LOG_LEVEL_DEBUG:
			fwprintf(filePtr, L"[%d/%d/%d %d:%d:%d][%s//%s//%010lld] %s\n", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, type, L"DEBUG", ++m_LogCount, message);
			break;
		case eLogLevel::LOG_LEVEL_ERROR:
			fwprintf(filePtr, L"[%d/%d/%d %d:%d:%d][%s//%s//%010lld] %s\n", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, type, L"ERROR", ++m_LogCount, message);
			break;
		case eLogLevel::LOG_LEVEL_SYSTEM:
			fwprintf(filePtr, L"[%d/%d/%d %d:%d:%d][%s//%s//%010lld] %s\n", today.tm_year - 100, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, type, L"SYSTEM", ++m_LogCount, message);
			break;
		}

		fclose(filePtr);
		
	}
#endif
	ReleaseSRWLockExclusive(&m_Lock);
}

void SysLog::WriteLogHex(const WCHAR* type, eLogLevel logLevel, const WCHAR* string, BYTE* byte, int32_t byteLen)
{
	//-----------------------------------------------------------------------
	// 	   HexLog Title문구 ex) RingBuffer : ByteLen[100] - 0xab 0xcd.....
	//-----------------------------------------------------------------------
	std::wstring str;
	WCHAR initStr[100] = { 0, };
	HRESULT result =StringCchPrintf(initStr, 100, L"%s:ByteLen[%d] -",string, byteLen);
	
	if (result == STRSAFE_E_INSUFFICIENT_BUFFER)
	{
		WriteLog(nullptr, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"Hex String 잘림");
	}

	str = initStr;


	for (int i = 0; i < byteLen; ++i)
	{
		WCHAR buffer[20] = { 0, };
		StringCchPrintf(buffer, 8, L" 0x%02x", *(byte +i));
		str += buffer;
	}

	WriteLog(type, logLevel, str.c_str());

}

void SysLog::SetDirectory(const WCHAR* directoryName)
{
	HRESULT result = StringCchPrintf(m_DirectoryName, MAX_PATH, L"./%s/",directoryName);

	if (-1 == _wmkdir(m_DirectoryName))
	{
		WriteLog(nullptr, eLogLevel::LOG_LEVEL_ERROR, L"Make Directory Fail :%d", GetLastError());
	}

}

void SysLog::SetLogLevel(eLogLevel logLevel)
{
	m_LogLevel = logLevel;
}
