#include "CPUUsage.h"

CPUUsage::CPUUsage(HANDLE process)
{
	//-----------------------------------------------
	// 	   프로세스 핸들 입력이 없다면 자기자신을 대상으로
	//-----------------------------------------------

	if (process == INVALID_HANDLE_VALUE)
	{
		m_Process = GetCurrentProcess();
	}
	//-----------------------------------------------
	// 	프로세서 갯수를 확인한다
	// 	프로세스 (exe) 실행을 계산시, CPU 갯수로 나누기를 하여 실제 사용율을 구함
	//-----------------------------------------------

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	m_NuberOfProcessors = sysInfo.dwNumberOfProcessors;


	m_ProcessorTotal = 0;
	m_ProcessorUser = 0;
	m_ProcessorKernel = 0;

	m_ProcessTotal = 0;
	m_ProcessUser = 0;
	m_ProcessKernel = 0;

	m_Processor_LastKernel.QuadPart = 0;
	m_Processor_LastUser.QuadPart = 0;
	m_Processor_LastIdle.QuadPart = 0;

	m_Process_LastUser.QuadPart = 0;
	m_Process_LastKernel.QuadPart = 0;
	m_Process_LastTime.QuadPart = 0;

	UpdateCPUTime();


}

void CPUUsage::UpdateCPUTime()
{
	//-------------------------------------------------------------
	// 	   프로세서 사용율을 갱신한다
	// 	   본래의 사용 구조체는 FILETIME 이지만, ULARGE_INTER와  구조가 같으므로 이를 사용함
	// 	   FILETIME 구조체는 100 나노세컨드 단위의 시간 단위를 표현하는 구조체임
	//-------------------------------------------------------------
	
	ULARGE_INTEGER idle;
	ULARGE_INTEGER kernel;
	ULARGE_INTEGER user;

	//-------------------------------------------------------------
	// 	   시스템 사용 시간을 구한다.
	// 	   아이들 타임 / 커널 사용 타임 (아이들포함) / 유저 사용타임
	//-------------------------------------------------------------

	if (GetSystemTimes((PFILETIME)&idle, (PFILETIME)&kernel, (PFILETIME)&user) == false)
	{
		return;
	}

	//-------------------------------------------------------------
	// 	  커널 타임에는  아이들 타임이 포함됨
	//-------------------------------------------------------------		
	ULONGLONG kernelDiff = kernel.QuadPart - m_Processor_LastKernel.QuadPart;
	ULONGLONG userDiff = user.QuadPart - m_Processor_LastUser.QuadPart;
	ULONGLONG idleDiff = idle.QuadPart - m_Processor_LastIdle.QuadPart;


	ULONGLONG total = kernelDiff + userDiff;
	ULONGLONG timeDiff;

	if (total == 0)
	{
		m_ProcessorUser = 0.0f;
		m_ProcessorKernel = 0.0f;
		m_ProcessorTotal = 0.0f;
	}
	else
	{
		// 커널타임에 아이들 타임이 있으므로 뻬서 계산
		m_ProcessorTotal = (float)((double)(total - idleDiff) / total * 100.0f);
		m_ProcessorUser = (float)((double)userDiff / total * 100.0f);
		m_ProcessorKernel = (float)((double)(kernelDiff - idleDiff) / total * 100.0f);
	}

	m_Processor_LastKernel = kernel;
	m_Processor_LastUser = user;
	m_Processor_LastIdle = idle;

	//-------------------------------------------------------------
	// 	  지정된 프로세스 사용률을 갱신한다
	//-------------------------------------------------------------	
	ULARGE_INTEGER none;
	ULARGE_INTEGER nowTime;

	//-------------------------------------------------------------
	// 	  현재의 100 나노 세컨드 단위 시간을 구한다 UTC 시간
	// 	   프로세스 사용율 판단의 공식
	// 
	// 	   a = 샘플간격의 시스템 시간을 구함(그냥 실제로 지나간 시간)
	// 	   b = 프로세스의 CPU사용 시간을 구함
	// 
	// 	   a: 100  = b: 사용율  
	//-------------------------------------------------------------	


	//-------------------------------------------------------------
	// 	 얼마의 시간이 지났는지 100나노 세컨드 시간을 구함
	//-------------------------------------------------------------
	GetSystemTimeAsFileTime((LPFILETIME)&nowTime);


	//-------------------------------------------------------------
	// 	 해당 프로세스가 사용한 시간을 구함
	// 	 두번째, 세번째는 실행, 종료 시간으로 미사용
	//-------------------------------------------------------------
	GetProcessTimes(m_Process,(LPFILETIME)&none, (LPFILETIME)&none, (LPFILETIME)&kernel, (LPFILETIME)&user);


	//-------------------------------------------------------------
	//	이전에 저장된 프로세스 시간과의 차를 구해서 실제로 얼마의 시간이 지났는지 확인
	// 	그리고 실제 지나온 시간으로 나누면 사용률이 나옴
	//-------------------------------------------------------------

	timeDiff = nowTime.QuadPart - m_Process_LastTime.QuadPart;
	userDiff = user.QuadPart - m_Process_LastUser.QuadPart;
	kernelDiff = kernel.QuadPart - m_Process_LastKernel.QuadPart;

	total = kernelDiff + userDiff;

	m_ProcessTotal = (float)(total / (double)m_NuberOfProcessors / (double)timeDiff * 100.0f);
	m_ProcessKernel = (float)(kernelDiff / (double)m_NuberOfProcessors / (double)timeDiff * 100.0f);
	m_ProcessUser = (float)(userDiff / (double)m_NuberOfProcessors / (double)timeDiff * 100.0f);

	m_Process_LastTime = nowTime;
	m_Process_LastKernel = kernel;
	m_Process_LastUser = user;

}

float CPUUsage::ProcessorTotal()
{
	return m_ProcessorTotal;
}

float CPUUsage::ProcessorUser()
{
	return m_ProcessorUser;
}

float CPUUsage::ProcessorKernel()
{
	return m_ProcessorKernel;
}

float CPUUsage::ProcessTotal()
{
	return m_ProcessTotal;
}

float CPUUsage::ProcessUser()
{
	return m_ProcessUser;
}

float CPUUsage::ProcessKernel()
{
	return m_ProcessKernel;
}
