#pragma once

class ObjectPoolBase
{
public:
	ObjectPoolBase() = default;
	virtual ~ObjectPoolBase() = default;

	virtual void* AllocMem(size_t size) = 0;
	virtual bool FreeMem(void* ptr) = 0;
};


struct ChunkMark
{
	void* _ChunkPtr;
	int64_t _MarkValue;
	size_t _Size;
	size_t _ThreadID;
};

template <typename T>
class ObjectPool : public ObjectPoolBase
{
	struct ChunkMemoryHeader
	{
	public:
		ChunkMemoryHeader() = default;
		~ChunkMemoryHeader() = default;
		ChunkMark _FrontMark;
		T _Data;
	};
	class ChunkMemory
	{
	public:

	public:
		ChunkMemory() = default;
		~ChunkMemory() = default;
	public:
		T* Alloc(size_t size = 0);
		void Free(T* data);


	public:
		void AllocInit(DWORD objectCount, ObjectPool<T>* centerPool);
	public:
		PointerStack<ChunkMemoryHeader*>* m_localChunk;
		LockFreeStack<ChunkMemoryHeader*> m_globalChunk;
		ObjectPool<T>* m_CenterMemoryPool;
		DWORD m_ObjectCount;
	};
public:
	ObjectPool();
	ObjectPool(DWORD objectCount);
	~ObjectPool();
public:

	virtual void* AllocMem(size_t size) override;
	virtual bool FreeMem(void* ptr) override;
	void FreeLoal(ChunkMemory* data);
	T* Alloc(size_t size = 0);
	bool Free(T* data);

	ChunkMemory* ChunkAlloc()
	{
		auto chunkPtr = m_ChunkMemoryPool.AllocateMemoryFromHeap();
		chunkPtr->m_localChunk = new PointerStack<ChunkMemoryHeader*>(m_ObjectCount);
		chunkPtr->AllocInit(m_ObjectCount, this);
		TlsSetValue(m_TLSChunkIndex, chunkPtr);

		return chunkPtr;
	}
	int32_t GetChunkCount();
	int32_t GetPoolCount();
	int32_t GetUseCount();

public:

	FreeList<ChunkMemory> m_ChunkMemoryPool;
	DWORD m_TLSChunkIndex;
	DWORD m_ObjectCount;
};


template<typename T>
inline T* ObjectPool<T>::ChunkMemory::Alloc(size_t size)
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
		ChunkMemoryHeader* data = nullptr;
		if(m_globalChunk.Pop(&data) == true)
		{
			m_localChunk->Push(data);
		}
		else
		{
			AllocInit(m_ObjectCount, m_CenterMemoryPool);
		}
	}

	return rtnData;
}

template<typename T>
inline void ObjectPool<T>::ChunkMemory::Free(T* data)
{
	ChunkMemoryHeader* dataPtr = reinterpret_cast<ChunkMemoryHeader*>(reinterpret_cast<char*>(data) - sizeof(ChunkMark));
	//----------------------------------------------------
	// 반납된 포인터가 언더플로우 한 경우
	//----------------------------------------------------
	if (dataPtr->_FrontMark._MarkValue != MARK_FRONT || dataPtr->_FrontMark._ChunkPtr != this)
	{
		throw(FreeListException(L"Underflow Violation", __LINE__));
		return;
	}

	if(dataPtr->_FrontMark._ThreadID == GetCurrentThreadId())
	{
		m_localChunk->Push(dataPtr);
	}
	else
	{
		m_globalChunk.Push(dataPtr);
	}
}

template <typename T>
void* ObjectPool<T>::AllocMem(size_t size)
{
	return reinterpret_cast<void*>(Alloc(size));
}

template <typename T>
bool ObjectPool<T>::FreeMem(void* ptr)
{
	return Free(static_cast<T*>(ptr));
}

template<typename T>
void ObjectPool<T>::ChunkMemory::AllocInit(DWORD objectCount, ObjectPool<T>* centerPool)
{
	if (m_localChunk->Empty())
	{
		m_CenterMemoryPool = centerPool;
		m_ObjectCount = objectCount;

		auto chunkHeader = static_cast<ChunkMemoryHeader*>(malloc(sizeof(ChunkMemoryHeader) * m_ObjectCount));
		for (size_t i = 0; i < m_ObjectCount; ++i)
		{
			chunkHeader[i]._FrontMark._ThreadID = GetCurrentThreadId();
			chunkHeader[i]._FrontMark._ChunkPtr = this;
			chunkHeader[i]._FrontMark._MarkValue = MARK_FRONT;
			m_localChunk->Push(&chunkHeader[i]);
		}
	}
}

template<typename T>
inline ObjectPool<T>::ObjectPool()
{
	m_ObjectCount = 50000;
	m_TLSChunkIndex = TlsAlloc();
	if (m_TLSChunkIndex == TLS_OUT_OF_INDEXES)
	{
		CRASH();
	}
	ChunkAlloc();
}

template<typename T>
inline ObjectPool<T>::ObjectPool(DWORD objectCount)
{
	m_ObjectCount = objectCount;
	m_TLSChunkIndex = TlsAlloc();
	if (m_TLSChunkIndex == TLS_OUT_OF_INDEXES)
	{
		CRASH();
	}
	ChunkAlloc();
}

template<typename T>
inline ObjectPool<T>::~ObjectPool()
{
	TlsFree(m_TLSChunkIndex);
}

template<typename T>
inline T* ObjectPool<T>::Alloc(size_t size)
{
	auto chunkPtr = static_cast<ChunkMemory*>(TlsGetValue(m_TLSChunkIndex));
	if (chunkPtr == nullptr)
	{
		chunkPtr = ChunkAlloc();
	}
	return chunkPtr->Alloc(size);
	
}

template<typename T>
inline bool ObjectPool<T>::Free(T* data)
{
	ObjectPool<T>::ChunkMemoryHeader* dataPtr= reinterpret_cast<ObjectPool<T>::ChunkMemoryHeader*>(reinterpret_cast<char*>(data) - sizeof(ChunkMark));
	reinterpret_cast<ChunkMemory*>(dataPtr->_FrontMark._ChunkPtr)->Free(data);

	return true;
}

template<typename T>
inline int32_t ObjectPool<T>::GetChunkCount()
{
	return m_ChunkMemoryPool.GetAllocCount();
}

template<typename T>
inline int32_t ObjectPool<T>::GetPoolCount()
{
	return m_ChunkMemoryPool.GetPoolCount();
}

template<typename T>
inline int32_t ObjectPool<T>::GetUseCount()
{
	return m_ChunkMemoryPool.GetUseCount();
}
