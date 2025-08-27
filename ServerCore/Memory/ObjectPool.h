#pragma once
template <typename T, int DEFAULT_SIZE>
class GlobalBatch
{
public:
	FreeList<PointerStack<ChunkSlot<T>*, DEFAULT_SIZE>>* m_pool;
	PointerStack<ChunkSlot<T>*, DEFAULT_SIZE>* m_slotStack;
};

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
	~ObjectPool();

public:
	virtual void* AllocFromChunk(size_t size) override;
	virtual void FreeToChunk(void* ptr) override;
	T* Alloc(size_t size = 0);
	bool Free(T* data);

	ChunkPage<T>* GetCurrentTLSChunk();
	ChunkPage<T>* ChunkAlloc();
	 void ChunkFree(ChunkPage<T>* chunk);
	int32_t GetChunkCount();
	int32_t GetPoolCount();
	int32_t GetUseCount();


	void RecycledToMyLocalPool(ChunkSlot<T>* memory);
	bool TryStealFromGlobal(PointerStack<ChunkSlot<T>*>& localChunk);
public:
	FreeList<ChunkPage<T>> m_chunkPool;
	LockFreeStack<GlobalBatch<T,5000>> m_globalBatchStack;
	DWORD m_TLSChunkIndex;
};

template <typename T>
void* ObjectPool<T>::AllocFromChunk(size_t size)
{
	auto chunk = GetCurrentTLSChunk();
	if (chunk == nullptr)
	{
		chunk = ChunkAlloc();
	}
	return chunk->Alloc(size);
}

template <typename T>
void ObjectPool<T>::FreeToChunk(void* ptr)
{
	ChunkSlot<T>* dataPtr = reinterpret_cast<ChunkSlot<T>*>(static_cast<char*>(ptr) - offsetof(ChunkSlot<T>, m_data));
	reinterpret_cast<ChunkPage<T>*>(dataPtr->m_metaData.m_chunkOwner)->Free(ptr);
}

template<typename T>
ObjectPool<T>::ObjectPool()
{
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
	auto chunkPtr = static_cast<ChunkPage<T>*>(TlsGetValue(m_TLSChunkIndex));
	if (chunkPtr == nullptr)
	{
		chunkPtr = ChunkAlloc();
	}
	return chunkPtr->Alloc(size);
}

template<typename T>
inline bool ObjectPool<T>::Free(T* data)
{
	ChunkSlot<T>* dataPtr= reinterpret_cast<ChunkSlot<T>*>(reinterpret_cast<char*>(data) - offsetof(ChunkSlot<T>,m_data));
	reinterpret_cast<ChunkPage<T>*>(dataPtr->m_metaData.m_chunkOwner)->Free(data);

	return true;
}

template <typename T>
ChunkPage<T>*  ObjectPool<T>::GetCurrentTLSChunk()
{
	return static_cast<ChunkPage<T>*>(TlsGetValue(m_TLSChunkIndex));
}

template <typename T>
ChunkPage<T>* ObjectPool<T>::ChunkAlloc()
{
	ChunkPage<T>* chunkPtr = m_chunkPool.Alloc();
	chunkPtr->SetOwner(this);

	TlsSetValue(m_TLSChunkIndex, chunkPtr);
	return chunkPtr;
}

template <typename T>
void ObjectPool<T>::ChunkFree(ChunkPage<T>* chunk)
{
	m_chunkPool.Free(chunk);
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

template <typename T>
void ObjectPool<T>::RecycledToMyLocalPool(ChunkSlot<T>* memory)
{
	auto tlsChunk = GetCurrentTLSChunk();
	if(tlsChunk == nullptr)
	{
		tlsChunk = ChunkAlloc();
	}

	tlsChunk->m_recycledSlots->Push(memory);
	if(tlsChunk->m_recycledSlots->Full())
	{
		m_globalBatchStack.Push({ &tlsChunk->m_recycledStackPool , tlsChunk->m_recycledSlots});
		tlsChunk->m_recycledSlots = tlsChunk->m_recycledStackPool.Alloc();
	}
}

template <typename T>
bool ObjectPool<T>::TryStealFromGlobal(PointerStack<ChunkSlot<T>*>& localChunk)
{
	GlobalBatch<T,5000> globalBatch;
	if(m_globalBatchStack.Pop(&globalBatch) == true)
	{
		std::swap(localChunk, *(globalBatch.m_slotStack));
		globalBatch.m_pool->Free(globalBatch.m_slotStack);
		return true;
	}
	return false;
}
