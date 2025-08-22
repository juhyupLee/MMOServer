#pragma once

class ObjectPoolBase
{
public:
	ObjectPoolBase() = default;
	virtual ~ObjectPoolBase() = default;

	virtual void* AllocFromChunk(size_t size) = 0;
	virtual void FreeToChunk(void* ptr) = 0;
};

template <typename T>
class ObjectPool : public ObjectPoolBase
{
public:
	ObjectPool();
	ObjectPool(DWORD objectCount);
	~ObjectPool();

public:
	virtual void* AllocFromChunk(size_t size) override;
	virtual void FreeToChunk(void* ptr) override;
	T* Alloc(size_t size = 0);
	bool Free(T* data);

	ChunkMemory<T>* ChunkSetting();
	int32_t GetChunkCount();
	int32_t GetPoolCount();
	int32_t GetUseCount();

public:
	FreeList<ChunkMemory<T>> m_chunkPool;
	DWORD m_TLSChunkIndex;
	DWORD m_ObjectCount;
};

template <typename T>
void* ObjectPool<T>::AllocFromChunk(size_t size)
{
	auto chunk = static_cast<ChunkMemory<T>*>(TlsGetValue(m_TLSChunkIndex));
	if (chunk == nullptr)
	{
		chunk = ChunkSetting();
	}
	return chunk->Alloc(size);
}

template <typename T>
void ObjectPool<T>::FreeToChunk(void* ptr)
{
	ChunkMemoryHeader<T>* dataPtr = reinterpret_cast<ChunkMemoryHeader<T>*>(static_cast<char*>(ptr) - sizeof(ChunkMark));
	reinterpret_cast<ChunkMemory<T>*>(dataPtr->_FrontMark._ChunkPtr)->Free(ptr);
}

template<typename T>
ObjectPool<T>::ObjectPool()
{
	m_ObjectCount = 50000;
	m_TLSChunkIndex = TlsAlloc();
	if (m_TLSChunkIndex == TLS_OUT_OF_INDEXES)
	{
		CRASH();
	}
	ChunkSetting();
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
	ChunkSetting();
}

template<typename T>
inline ObjectPool<T>::~ObjectPool()
{
	TlsFree(m_TLSChunkIndex);
}

template<typename T>
inline T* ObjectPool<T>::Alloc(size_t size)
{
	auto chunkPtr = static_cast<ChunkMemory<T>*>(TlsGetValue(m_TLSChunkIndex));
	if (chunkPtr == nullptr)
	{
		chunkPtr = ChunkSetting();
	}
	return chunkPtr->Alloc(size);
	
}

template<typename T>
inline bool ObjectPool<T>::Free(T* data)
{
	ChunkMemoryHeader<T>* dataPtr= reinterpret_cast<ChunkMemoryHeader<T>*>(reinterpret_cast<char*>(data) - sizeof(ChunkMark));
	reinterpret_cast<ChunkMemory<T>*>(dataPtr->_FrontMark._ChunkPtr)->Free(data);

	return true;
}

template <typename T>
ChunkMemory<T>* ObjectPool<T>::ChunkSetting()
{
	auto chunkPtr = m_chunkPool.Alloc();
	chunkPtr->ChunkInit(m_ObjectCount);
	TlsSetValue(m_TLSChunkIndex, chunkPtr);
	return chunkPtr;
}

template<typename T>
inline int32_t ObjectPool<T>::GetChunkCount()
{
	return m_chunkPool.GetAllocCount();
}

template<typename T>
inline int32_t ObjectPool<T>::GetPoolCount()
{
	return m_chunkPool.GetPoolCount();
}

template<typename T>
inline int32_t ObjectPool<T>::GetUseCount()
{
	return m_chunkPool.GetUseCount();
}
