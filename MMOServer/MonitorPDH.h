#pragma once
#include <iostream>
#include <Windows.h>
#pragma comment(lib,"Pdh.lib")
#include <Pdh.h>

class MonitorPDH
{
public:
	MonitorPDH();
	void ReNewPDH();

	int64_t GetPrivateMemory();
	int64_t GetNPMemory();
	double GetAvailableMemory();

	int64_t GetTCPRetrans();
private:
	PDH_HQUERY m_MemQuery;
	PDH_HCOUNTER m_MemCounter;

	PDH_HQUERY m_NPMemQuery;
	PDH_HCOUNTER m_NPMemCounter;

	PDH_HQUERY m_AvailableMemQuery;
	PDH_HCOUNTER m_AvailableMemCounter;

	PDH_HQUERY m_TCPRetransQuery;
	PDH_HCOUNTER m_TCPRetransCounter;
	
	//\TCPv4\Segments Retransmitted / sec
};