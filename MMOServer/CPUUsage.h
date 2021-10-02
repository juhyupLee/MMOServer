#pragma once
#include <Windows.h>

class CPUUsage
{
public:
	CPUUsage(HANDLE process = INVALID_HANDLE_VALUE);
	
	void UpdateCPUTime();
	float ProcessorTotal();
	float ProcessorUser();
	float ProcessorKernel();

	float ProcessTotal();
	float ProcessUser();
	float ProcessKernel();
private:
	HANDLE m_Process;
	int m_NuberOfProcessors;

	float m_ProcessorTotal;
	float m_ProcessorUser;
	float m_ProcessorKernel;

	float m_ProcessTotal;
	float m_ProcessUser;
	float m_ProcessKernel;

	ULARGE_INTEGER m_Processor_LastKernel;
	ULARGE_INTEGER m_Processor_LastUser;
	ULARGE_INTEGER m_Processor_LastIdle;
	
	ULARGE_INTEGER m_Process_LastKernel;
	ULARGE_INTEGER m_Process_LastUser;
	ULARGE_INTEGER m_Process_LastTime;

};