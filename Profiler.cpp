#include "Profiler.h"
#include <stdio.h>
#include <cmath>
#include <Windows.h>
#include <iostream>

char g_First = 0;

void ProfileDataOutText(const WCHAR* fileName)
{

}

void ProfileReset()
{

}

void Profiler::ProfileBegin(const WCHAR* name)
{
	LARGE_INTEGER beginTime;
	QueryPerformanceCounter(&beginTime);

	ProfileInfo* profileInfo = (ProfileInfo*)TlsGetValue(m_TlsIndex);
	if (profileInfo == nullptr)
	{
		profileInfo = new ProfileInfo[100];
		m_ProfileManager[InterlockedIncrement(&m_ProfileIndex)] = profileInfo;
		TlsSetValue(m_TlsIndex, profileInfo);
	}

	bool bRedundant = false;
	int redundantIndex = 0;
	//태그가 이미 존재하는지 먼저 체크 
	for (int index = 0; index < TAG_MAX; ++index)
	{
		if (!wcscmp(profileInfo[index]._TagName, name))
		{
			bRedundant = true;
			redundantIndex = index;
			break;
		}
	}
	//태그가 이미 존재하는거라면 , 시간만 갱신.
	if (bRedundant)
	{		
		profileInfo[redundantIndex]._SampleData._StartTime.QuadPart = beginTime.QuadPart;	
	}
	else
	{
		//그렇지 않으면 신규등록.
		for (int index = 0; index < TAG_MAX; ++index)
		{
			if (profileInfo[index]._bUsed == false)
			{
				//Profile Begin에서는 사용유무와, 이름, 시작시간을 체크한다.
				profileInfo[index]._bUsed = true;

				//---------------------------------------------------
				// Profile : tag명
				//---------------------------------------------------
				wcscpy_s(profileInfo[index]._TagName, sizeof(profileInfo[index]._TagName)/2, name);
				profileInfo[index]._ThreadID = GetCurrentThreadId();
				profileInfo[index]._SampleData._StartTime.QuadPart = beginTime.QuadPart;
				break;
			}
		}

	}
	

}
void Profiler::ProfileEnd(const WCHAR* name)
{
	bool bRedundant = false;
	int redundantIndex = 0;
	LARGE_INTEGER endTime;
	QueryPerformanceCounter(&endTime);


	ProfileInfo* profileInfo = (ProfileInfo*)TlsGetValue(m_TlsIndex);
	if (profileInfo == nullptr)
	{
		Crash();
	}

	//태그가 이미 존재하는지 먼저 체크 
	for (int index = 0; index < TAG_MAX; ++index)
	{
		if (!wcscmp(profileInfo[index]._TagName, name))
		{
			bRedundant = true;
			redundantIndex = index;
			break;
		}
	}

	if (!bRedundant)
	{
		Crash();
	}

	int64_t workingTime = 0;

	workingTime = endTime.QuadPart - profileInfo[redundantIndex]._SampleData._StartTime.QuadPart;
	profileInfo[redundantIndex]._SampleData._TotalTime += workingTime;
	profileInfo[redundantIndex]._SampleData._Call++;


	// Max 1,2 Setting
	if (profileInfo[redundantIndex]._SampleData._Max[0] < workingTime)
	{
		profileInfo[redundantIndex]._SampleData._Max[1] = profileInfo[redundantIndex]._SampleData._Max[0];
		profileInfo[redundantIndex]._SampleData._Max[0] = workingTime;
	}
	else
	{
		if (profileInfo[redundantIndex]._SampleData._Max[1] < workingTime)
		{
			profileInfo[redundantIndex]._SampleData._Max[1] = workingTime;
		}
	}

	// Min 1,2 Setting
	if (profileInfo[redundantIndex]._SampleData._Min[0] > workingTime)
	{
		profileInfo[redundantIndex]._SampleData._Min[1] = profileInfo[redundantIndex]._SampleData._Min[0];
		profileInfo[redundantIndex]._SampleData._Min[0] = workingTime;
	}
	else
	{
		if (profileInfo[redundantIndex]._SampleData._Min[1] > workingTime)
		{
			profileInfo[redundantIndex]._SampleData._Min[1] = workingTime;
		}
	}


	//TlsSetValue(m_TlsIndex, profileInfo);


}

//void Profiler::ProfileEnd(const WCHAR* name)
//{
//	
//	bool bRedundant = false;
//	int redundantIndex = 0;
//	LARGE_INTEGER endTime;
//	QueryPerformanceCounter(&endTime);
//
//	DWORD threadID = GetCurrentThreadId();
//
//	//태그가 이미 존재하는지 먼저 체크 
//	for (int index = 0; index < INFO_CNT; ++index)
//	{
//
//		if (!wcscmp(m_ProfileInfo[index]._TagName, name))
//		{
//			bRedundant = true;
//			redundantIndex = index;
//			break;
//		}
//	}
//
//	if (!bRedundant)
//	{
//		return;
//	}
//
//	bool bSearch = false;
//	int64_t workingTime = 0;
//
//	for (int i = 0; i < 100; ++i)
//	{
//		if (m_ProfileInfo[redundantIndex]._SampleData[i]._ThreadID == threadID)
//		{
//			workingTime = endTime.QuadPart - m_ProfileInfo[redundantIndex]._SampleData[i]._StartTime.QuadPart;
//			m_ProfileInfo[redundantIndex]._SampleData[i]._TotalTime += workingTime;
//			m_ProfileInfo[redundantIndex]._SampleData[i]._Call++;
//
//			bSearch = true;
//
//
//
//			// Max 1,2 Setting
//			if (m_ProfileInfo[redundantIndex]._SampleData[i]._Max[0] < workingTime)
//			{
//				m_ProfileInfo[redundantIndex]._SampleData[i]._Max[1] = m_ProfileInfo[redundantIndex]._SampleData[i]._Max[0];
//				m_ProfileInfo[redundantIndex]._SampleData[i]._Max[0] = workingTime;
//			}
//			else
//			{
//				if (m_ProfileInfo[redundantIndex]._SampleData[i]._Max[1] < workingTime)
//				{
//					m_ProfileInfo[redundantIndex]._SampleData[i]._Max[1] = workingTime;
//				}
//			}
//
//			// Min 1,2 Setting
//			if (m_ProfileInfo[redundantIndex]._SampleData[i]._Min[0] > workingTime)
//			{
//				m_ProfileInfo[redundantIndex]._SampleData[i]._Min[1] = m_ProfileInfo[redundantIndex]._SampleData[i]._Min[0];
//				m_ProfileInfo[redundantIndex]._SampleData[i]._Min[0] = workingTime;
//			}
//			else
//			{
//				if (m_ProfileInfo[redundantIndex]._SampleData[i]._Min[1] > workingTime)
//				{
//					m_ProfileInfo[redundantIndex]._SampleData[i]._Min[1] = workingTime;
//				}
//			}
//	
//			break;
//		}
//	}
//
//	if (!bSearch)
//	{
//		return;
//	}
//
//
//}

void Profiler::ProfileDataOutText(const WCHAR* fileName)
{
	FILE* pFile = nullptr;
	errno_t error = _wfopen_s(&pFile, fileName, L"w,ccs=UTF-16LE");

	while (error != 0)
	{
		error = _wfopen_s(&pFile, fileName, L"w,ccs=UTF-16LE");
	}
	fwprintf(pFile, L"--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
	fwprintf(pFile, L"%30s | %30s | %34s | %34s | %34s | %30s|\n",L"ThreadID", L"Name", L"Average", L"Min", L"Max", L"Call");
	fwprintf(pFile, L"--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
	for (int profileCnt = 0; profileCnt <= m_ProfileIndex; ++profileCnt)
	{
		int a = 10;
		for (int tagNo = 0; tagNo <TAG_MAX ;++tagNo)
		{
			ProfileInfo* tempInfo = *(m_ProfileManager + profileCnt) + tagNo;
			if (tempInfo->_bUsed)
			{
				__int64 valuableResult = tempInfo->_SampleData._TotalTime-
					(tempInfo->_SampleData._Max[0] +
						tempInfo->_SampleData._Max[1] +
						tempInfo->_SampleData._Min[0] +
						tempInfo->_SampleData._Min[1]);

				double average = ((double)valuableResult / (((double)tempInfo->_SampleData._Call - (double)4))) / CONVERT_US;
				double min = tempInfo->_SampleData._Min[0] / CONVERT_US;
				double max = tempInfo->_SampleData._Max[0] / CONVERT_US;

				fwprintf(pFile, L"%30ld | %30s | %30.4lf[us] | %30.4lf[us] | %30.4lf[us] | %30lld|\n",
					tempInfo->_ThreadID,
					tempInfo->_TagName,
					average,
					min,
					max,
					tempInfo->_SampleData._Call);
			}
		}
		fwprintf(pFile, L"--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
	}

	fclose(pFile);
}

void Profiler::ProfileReset()
{
	//for (int index = 0; index < INFO_CNT; ++index)
	//{
	//	if (g_ProfileInfo[index].m_bUsed)
	//	{
	//		g_ProfileInfo[index].m_Call = 0;
	//		g_ProfileInfo[index].m_Max[0] = MAX;
	//		g_ProfileInfo[index].m_Max[1] = MAX;
	//		g_ProfileInfo[index].m_Min[0] = MIN;
	//		g_ProfileInfo[index].m_Min[1] = MIN;
	//		g_ProfileInfo[index].m_TotalTime = 0;
	//		g_ProfileInfo[index].m_StartTime = { 0 };

	//	}
	//}
}

void Profiler::Crash()
{
	int* p = nullptr;
	*p = 10;

}
