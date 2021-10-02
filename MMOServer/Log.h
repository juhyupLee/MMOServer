#pragma once
#include <iostream>
#include <Windows.h>
#include <unordered_map>
#include <string>

//--------------------------------------------------------------------------------------------------------------------------------
//프로세스 켜짐과 동시에 파일이 생성되며, 모든 로그는 하나에 파일에 다 저장되는 모드이다
// 0이면 Type에따라 로그 파일이 생성되며, 월마다 갱신된다. 프로세스가 종료되도, 파일을 새로 생성하지않고, 로그파일에 계속 추가한다
//--------------------------------------------------------------------------------------------------------------------------------
#define SERVER_ON_LOG 1
//--------------------------------------------------------------------------------------------------------------------------------						

#define _LOG  SysLog::GetInstance()

class SysLog
{
public:
	SysLog();
	~SysLog();

	struct LogFileName
	{
		std::wstring _FileName; //로그 파일 이름 
		int32_t _LastMonth;		//월 단위로 갱신해주기위해 저장
	};
	enum class eLogLevel
	{
		LOG_LEVEL_DEBUG,
		LOG_LEVEL_ERROR,
		LOG_LEVEL_SYSTEM
	};
	enum
	{
		BUFFER_SIZE = 1000
	};
	
public:
	static SysLog* GetInstance();
private:
	eLogLevel m_LogLevel;
	WCHAR m_ServerONLogFileName[MAX_PATH];
	
	WCHAR m_DirectoryName[MAX_PATH];
	//std::unordered_map<std::wstring, std::wstring> m_FileNameMap;
	std::unordered_map<std::wstring, LogFileName*> m_FileNameMap;

public:
	void WriteLog(const WCHAR* type, eLogLevel logLevel, const WCHAR* stringFormat, ...);
	void WriteLogHex(const WCHAR* type, eLogLevel logLevel, const WCHAR* string,BYTE* byte, int32_t byteLen);
	void SetDirectory(const WCHAR* directoryName);
	void SetLogLevel(eLogLevel logLevel);
	SRWLOCK m_Lock;
	uint64_t m_LogCount;

};

