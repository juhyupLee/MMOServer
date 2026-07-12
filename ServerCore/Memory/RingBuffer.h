#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

struct MutableBufferSegment
{
	char* data{ nullptr };
	std::size_t size{ 0 };
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
	//void GetDirectDeQData(DirectData* directSize, Session* curSession);
	//----------------------------------------------------------


	int Enqueue(char* buffer, int32_t size);
	int Dequeue(char* buffer, int32_t size);

	int32_t GetWriteSize() const;
	int32_t GetReadSize() const;


	// The writable part of a ring buffer can wrap at the physical end, so at
	// most two platform-neutral segments are exposed to the socket layer.
	std::array<MutableBufferSegment, 2> GetWritableSegments() noexcept;

	int32_t GetDirectWriteSize()const;
	int32_t GetDirectReadSize()const;

	void ClearBuffer();

	bool CommitWrite(std::size_t size) noexcept;
	void MoveWitePosition(int size); // Legacy spelling kept for existing callers.
	void MoveReadPosition(int size);


	char* GetFrontBufferPtr(int32_t copyFront, int32_t copyRear);
	char* GetWriteBufferPtr(int32_t copyFront, int32_t copyRear);
	char* GetReadBufferPtr(void);
	char* GetWriteBufferPtr(void);
	int Peek(char* dest, int size);

	int32_t GetReadPosition();
	int32_t GetwritePosition();

	int32_t GetDirectEnqueueSize(int32_t copyFront, int32_t copyRear)const;
	int32_t GetDirectDequeueSize(int32_t copyFront, int32_t copyRear)const;

private:
	int32_t GetFreeSize(int32_t front, int32_t rear) const;
	int32_t GetUsedSize(int32_t front, int32_t rear) const;

private:
	int32_t m_readPosition;
	int32_t m_writePosition;
	char m_Buffer[RING_BUFFER_SIZE];
};
