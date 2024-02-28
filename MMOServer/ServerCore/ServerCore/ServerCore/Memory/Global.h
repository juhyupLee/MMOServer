#pragma once
#define dfPACKET_CODE		0x77
#define dfPACKET_KEY		0x32

void CRASH();

class MyLock
{
public:
	MyLock()
	{
		InitializeSRWLock(&m_Lock);
	}
	void Lock()
	{
		AcquireSRWLockExclusive(&m_Lock);
	}
	void Unlock()
	{
		ReleaseSRWLockExclusive(&m_Lock);
	}
private:
	SRWLOCK m_Lock;
};

