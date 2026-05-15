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

template <typename T, int32_t SLOT_COUNT>
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
	bool TryStealFromGlobal(PointerStack<ChunkSlot<T>*, SLOT_COUNT>& localChunk);
public:
	FreeList<ChunkPage<T>> m_chunkPool;
	LockFreeStack<GlobalBatch<T, SLOT_COUNT>> m_globalBatchStack;

	// thread_local chunk per ObjectPool instance
	// We use a static thread_local map keyed by ObjectPool pointer
	static thread_local std::unordered_map<void*, ChunkPage<T>*> s_tlsChunkMap;
};

template<typename T, int32_t SLOT_COUNT>
thread_local std::unordered_map<void*, ChunkPage<T>*> ObjectPool<T, SLOT_COUNT>::s_tlsChunkMap;

template<typename T, int32_t SLOT_COUNT >
void* ObjectPool<T, SLOT_COUNT>::AllocFromChunk(size_t size)
{
	auto chunk = GetCurrentTLSChunk();
	if (chunk == nullptr)
	{
		chunk = ChunkAlloc();
	}
	return chunk->Alloc(size);
}

template<typename T, int32_t SLOT_COUNT>
void ObjectPool<T, SLOT_COUNT>::FreeToChunk(void* ptr)
{
	ChunkSlot<T>* chunkSlot = reinterpret_cast<ChunkSlot<T>*>(static_cast<char*>(ptr) - offsetof(ChunkSlot<T>, m_data));
	reinterpret_cast<ChunkPage<T>*>(chunkSlot->m_metaData.m_chunkOwner)->Free(ptr);
}

template<typename T, int32_t SLOT_COUNT>
ObjectPool<T, SLOT_COUNT>::ObjectPool()
{
	ChunkAlloc();
}

template<typename T, int32_t SLOT_COUNT>
inline ObjectPool<T, SLOT_COUNT>::~ObjectPool()
{
}

template<typename T, int32_t SLOT_COUNT>
inline T* ObjectPool<T, SLOT_COUNT>::Alloc(size_t size)
{
	auto chunkPage = GetCurrentTLSChunk();
	if (chunkPage == nullptr)
	{
		chunkPage = ChunkAlloc();
	}
	return chunkPage->Alloc(size);
}

template<typename T, int32_t SLOT_COUNT>
inline bool ObjectPool<T, SLOT_COUNT>::Free(T* data)
{
	ChunkSlot<T>* chunkSlot= reinterpret_cast<ChunkSlot<T>*>(reinterpret_cast<char*>(data) - offsetof(ChunkSlot<T>,m_data));
	reinterpret_cast<ChunkPage<T>*>(chunkSlot->m_metaData.m_chunkOwner)->Free(data);

	return true;
}

template<typename T, int32_t SLOT_COUNT>
ChunkPage<T>* ObjectPool<T, SLOT_COUNT>::GetCurrentTLSChunk()
{
	auto it = s_tlsChunkMap.find(this);
	if (it != s_tlsChunkMap.end())
	{
		return it->second;
	}
	return nullptr;
}

template<typename T, int32_t SLOT_COUNT>
ChunkPage<T>* ObjectPool<T, SLOT_COUNT>::ChunkAlloc()
{
	ChunkPage<T>* chunkPtr = m_chunkPool.Alloc();
	chunkPtr->SetOwner(this);

	s_tlsChunkMap[this] = chunkPtr;
	return chunkPtr;
}

template<typename T, int32_t SLOT_COUNT>
void ObjectPool<T, SLOT_COUNT>::ChunkFree(ChunkPage<T>* chunk)
{
	m_chunkPool.Free(chunk);
}

template<typename T, int32_t SLOT_COUNT>
inline int32_t ObjectPool<T, SLOT_COUNT>::GetChunkCount()
{
	return m_chunkPool.GetAllocCount();
}

template<typename T, int32_t SLOT_COUNT>
inline int32_t ObjectPool<T, SLOT_COUNT>::GetPoolCount()
{
	return m_chunkPool.GetPoolCount();
}

template<typename T, int32_t SLOT_COUNT>
inline int32_t ObjectPool<T, SLOT_COUNT>::GetUseCount()
{
	return m_chunkPool.GetUseCount();
}

template<typename T, int32_t SLOT_COUNT>
void ObjectPool<T, SLOT_COUNT>::RecycledToMyLocalPool(ChunkSlot<T>* memory)
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

template<typename T, int32_t SLOT_COUNT>
bool ObjectPool<T, SLOT_COUNT>::TryStealFromGlobal(PointerStack<ChunkSlot<T>*, SLOT_COUNT>& localChunk)
{
	GlobalBatch<T, SLOT_COUNT> globalBatch;
	if(m_globalBatchStack.Pop(&globalBatch) == true)
	{
		std::swap(localChunk, *(globalBatch.m_slotStack));
		globalBatch.m_pool->Free(globalBatch.m_slotStack);
		return true;
	}
	return false;
}
