#pragma once

struct ChunkMark
{
	void* _ChunkPtr;
	int64_t _MarkValue;
	size_t _Size;
	size_t _ThreadID;
};
template <typename T>
struct ChunkMemoryHeader
{
public:
	ChunkMemoryHeader() = default;
	~ChunkMemoryHeader() = default;
	ChunkMark _FrontMark;
	T _Data;
};

template <typename T>
class ChunkMemory
{
public:
	ChunkMemory() = default;
	~ChunkMemory() = default;
public:
	T* Alloc(size_t size = 0);
	void Free(void* data);

public:
	void ChunkInit(DWORD objectCount);
public:
	PointerStack<ChunkMemoryHeader<T>*>* m_localChunk{nullptr};
	LockFreeStack<ChunkMemoryHeader<T>*> m_globalChunk;
	DWORD m_ObjectCount;
};

template<typename T>
 T* ChunkMemory<T>::Alloc(size_t size)
{
	//-------------------------------------
	// Chunk의 Mark부분을 제외한 진짜  T타입의 포인터를 던져준다.
	//------------------------------------
	auto chunkHeader = m_localChunk->Pop();
	T* rtnData = reinterpret_cast<T*>(reinterpret_cast<char*>(chunkHeader) + sizeof(ChunkMark));
	chunkHeader->_FrontMark._Size = size;
	chunkHeader->_FrontMark._ThreadID = GetCurrentThreadId();

	if(m_localChunk->Empty())
	{
		ChunkMemoryHeader<T>* data = nullptr;
		if(m_globalChunk.Pop(&data) == true)
		{
			m_localChunk->Push(data);
		}
		else
		{
			ChunkInit(m_ObjectCount);
		}
	}

	return rtnData;
}

template<typename T>
void ChunkMemory<T>::Free(void* data)
{
	ChunkMemoryHeader<T>* dataPtr = reinterpret_cast<ChunkMemoryHeader<T>*>(reinterpret_cast<char*>(data) - sizeof(ChunkMark));
	//----------------------------------------------------
	// 반납된 포인터가 언더플로우 한 경우
	//----------------------------------------------------
	if (dataPtr->_FrontMark._MarkValue != MARK_FRONT || dataPtr->_FrontMark._ChunkPtr != this)
	{
		throw(FreeListException(L"Underflow Violation", __LINE__));
	}

	if(dataPtr->_FrontMark._ThreadID == GetCurrentThreadId())
	{
		m_localChunk->Push(dataPtr);
	}
	else
	{
		m_globalChunk.Push(dataPtr);
		if(m_ObjectCount <=  m_globalChunk.GetStackCount())
		{
			//m_centerPool->Alloc()
		}
	}
}


template <typename T>
void ChunkMemory<T>::ChunkInit(DWORD objectCount)
{
	m_ObjectCount = objectCount;
	if(m_localChunk == nullptr)
	{
		m_localChunk = new PointerStack<ChunkMemoryHeader<T>*>(m_ObjectCount);
	}

	if (m_localChunk->Empty())
	{
		auto chunkHeader = static_cast<ChunkMemoryHeader<T>*>(malloc(sizeof(ChunkMemoryHeader<T>) * m_ObjectCount));
		for (size_t i = 0; i < m_ObjectCount; ++i)
		{
			chunkHeader[i]._FrontMark._ThreadID = GetCurrentThreadId();
			chunkHeader[i]._FrontMark._ChunkPtr = this;
			chunkHeader[i]._FrontMark._MarkValue = MARK_FRONT;
			m_localChunk->Push(&chunkHeader[i]);
		}
	}
}
