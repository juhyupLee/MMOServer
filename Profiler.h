#pragma once
#include <Windows.h>
#include <iostream>

#define CONVERT_US 10.0

struct ProfileData
{
	ProfileData()
		:
		_StartTime{ 0 },
		_TotalTime(0),
		_Min{ MAXINT64,MAXINT64 },
		_Max{ MININT64,MININT64 },
		_Call(0)
	{

	}
	LARGE_INTEGER _StartTime;
	int64_t _TotalTime;
	int64_t _Min[2];
	int64_t _Max[2];
	int64_t _Call;
};
struct ProfileInfo
{
	ProfileInfo()
	{
		_bUsed = false;
		_ThreadID = 0;
		ZeroMemory(_TagName, 256);
	}
	WCHAR _TagName[128];
	ProfileData _SampleData;
	bool _bUsed;
	DWORD _ThreadID;


};



class Profiler
{
public:
	enum
	{
		MAX_INFO =100,
		TAG_MAX =100
	};
	Profiler()
		:
		m_ProfileManager{nullptr,}
	{
		QueryPerformanceFrequency(&m_Frequency);
		m_TlsIndex = TlsAlloc();
		if (m_TlsIndex == TLS_OUT_OF_INDEXES)
		{
			Crash();
		}
		m_ProfileIndex = -1;
	}
	~Profiler()
	{
		TlsFree(m_TlsIndex);
		for (int i = 0; i <= m_ProfileIndex; ++i)
		{
			delete[] m_ProfileManager[i];
		}
	}

public:
	void ProfileBegin(const WCHAR* name);
	void ProfileEnd(const WCHAR* name);
	void ProfileDataOutText(const WCHAR* fileName);
	void ProfileReset();

	void Crash();
private:
	LARGE_INTEGER m_Frequency;
	DWORD m_TlsIndex;
	ProfileInfo* m_ProfileManager[MAX_INFO];
	LONG m_ProfileIndex;



};