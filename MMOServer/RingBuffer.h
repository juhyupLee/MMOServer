#pragma once
#include <iostream>
#include <Windows.h>

struct Session;

struct DirectData
{
	int32_t _Direct1;
	int32_t _Direct2;
	char* bufferPtr1;
	char* bufferPtr2;
};
class RingQ
{
public:
	enum
	{
		RING_BUFFER_SIZE = 7000
	};

public:
	RingQ();
	~RingQ();
public:
	//----------------------------------------------------------
	// For Debug
	//----------------------------------------------------------
	int Enqueue(char* buffer, int32_t size, Session* curSession);
	void GetDirectDeQData(DirectData* directSize, Session* curSession);
	void MoveFrontTest(int size, Session* curSession);
	//----------------------------------------------------------

	
	int Enqueue(char* buffer, int32_t size);
	int Dequeue(char* buffer, int32_t size);

	int32_t GetFreeSize() const; 
	int32_t GetUsedSize() const;


	void GetDirectEnQData(DirectData* directSize);
	
	void GetDirectDeQData(DirectData* directSize);

	int32_t GetDirectEnqueueSize()const;
	int32_t GetDirectDequeueSize()const;

	void ClearBuffer();

	void MoveRear(int size);
	void MoveFront(int size);

	
	char* GetFrontBufferPtr(int32_t copyFront, int32_t copyRear);
	char* GetRearBufferPtr(int32_t copyFront, int32_t copyRear);
	char* GetFrontBufferPtr(void);
	char* GetRearBufferPtr(void);
	int Peek(char* dest, int size);

	int32_t GetFront();
	int32_t GetRear();

	int32_t GetDirectEnqueueSize(int32_t copyFront, int32_t copyRear)const;
	int32_t GetDirectDequeueSize(int32_t copyFront, int32_t copyRear)const;

private:
	int32_t GetFreeSize(int32_t front, int32_t rear) const;
	int32_t GetUsedSize(int32_t front, int32_t rear) const;

private:
	int32_t m_Front;
	int32_t m_Rear;
	char m_Buffer[RING_BUFFER_SIZE];
};