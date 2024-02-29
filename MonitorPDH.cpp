#include "MonitorPDH.h"


#include <psapi.h>
#include <string>
MonitorPDH::MonitorPDH()
{
	//L"\\Memory\\Pool Nonpaged Bytes"
	//L"\\Memory\\Available MBytes"

	WCHAR memQueryStr[500];
	WCHAR processName[MAX_PATH] = L"PDH";

	GetModuleBaseName(GetCurrentProcess(), NULL, processName, MAX_PATH);

	for (int i = 0; i < MAX_PATH; ++i)
	{
		if (L'.' == processName[i])
		{
			processName[i] = NULL;
			break;
		}
	}
	//-----------------------------------------------------------
	// 	   Private Memory 
	//-----------------------------------------------------------
	PdhOpenQuery(NULL, NULL, &m_MemQuery);

	wsprintf(memQueryStr, L"\\Process(%s)\\Private Bytes", processName);

	PdhAddCounter(m_MemQuery, memQueryStr, NULL, &m_MemCounter);
	PdhCollectQueryData(m_MemQuery);


	//-----------------------------------------------------------
	// 	   Non Page Memory - System
	//-----------------------------------------------------------
	PdhOpenQuery(NULL, NULL, &m_NPMemQuery);

	wsprintf(memQueryStr, L"\\Memory\\Pool Nonpaged Bytes");

	PdhAddCounter(m_NPMemQuery, memQueryStr, NULL, &m_NPMemCounter);
	PdhCollectQueryData(m_NPMemQuery);

	//-----------------------------------------------------------
	// 	   Available Memory - System
	//-----------------------------------------------------------
	PdhOpenQuery(NULL, NULL, &m_AvailableMemQuery);

	wsprintf(memQueryStr, L"\\Memory\\Available MBytes");

	PdhAddCounter(m_AvailableMemQuery, memQueryStr, NULL, &m_AvailableMemCounter);
	PdhCollectQueryData(m_AvailableMemQuery);


	//-----------------------------------------------------------
	// 	   TCP Retransmission
	//-----------------------------------------------------------
	PdhOpenQuery(NULL, NULL, &m_TCPRetransQuery);
	
	wsprintf(memQueryStr, L"\\TCPv4\\Segments Retransmitted/sec");

	PdhAddCounter(m_TCPRetransQuery, memQueryStr, NULL, &m_TCPRetransCounter);
	PdhCollectQueryData(m_TCPRetransQuery);

}

void MonitorPDH::ReNewPDH()
{
	PdhCollectQueryData(m_MemQuery);
	PdhCollectQueryData(m_NPMemQuery);
	PdhCollectQueryData(m_AvailableMemQuery);
	PdhCollectQueryData(m_TCPRetransQuery);
}

int64_t MonitorPDH::GetPrivateMemory()
{
	PDH_FMT_COUNTERVALUE counterVal;
	PdhGetFormattedCounterValue(m_MemCounter, PDH_FMT_LARGE, NULL, &counterVal);

	return counterVal.largeValue;
}


int64_t MonitorPDH::GetNPMemory()
{
	PDH_FMT_COUNTERVALUE counterVal;
	PdhGetFormattedCounterValue(m_NPMemCounter, PDH_FMT_LARGE, NULL, &counterVal);

	return counterVal.largeValue;
}

double MonitorPDH::GetAvailableMemory()
{
	PDH_FMT_COUNTERVALUE counterVal;
	PdhGetFormattedCounterValue(m_AvailableMemCounter, PDH_FMT_DOUBLE, NULL, &counterVal);

	return counterVal.doubleValue;
}

int64_t MonitorPDH::GetTCPRetrans()
{
	PDH_FMT_COUNTERVALUE counterVal;
	PdhGetFormattedCounterValue(m_TCPRetransCounter, PDH_FMT_LARGE, NULL, &counterVal);
	return counterVal.largeValue;
}
