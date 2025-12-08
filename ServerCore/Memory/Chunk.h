#pragma once

constexpr size_t TargetChunkSize = 128 * 1024; // 64KB

template<typename T>
constexpr size_t CalcSlotCount()
{

	constexpr size_t s = sizeof(T);
	// 2MB를 기본 정렬/상한 축으로 활용
	constexpr size_t KB = 1024;
	constexpr size_t MB = 1024 * KB;

	if (s <= 64)    return (256 * KB) / s;     // 32B→8,192 slots
	if (s <= 128)   return (1 * MB) / s;     // 128B→8,192
	if (s <= 256)   return (1 * MB) / s;     // 256B→4,096
	if (s <= 512)   return (2 * MB) / s;     // 512B→4,096
	if (s <= 1024)  return (2 * MB) / s;     // 1KB→2,048

	// 큰 객체: 2MB 상한 + 최소 슬롯 보장
	constexpr size_t n = (2 * MB) / s;
	return n >= 64 ? n : 64;
}

template<typename T, int SLOT_COUNT = CalcSlotCount<T>()>
class ObjectPool;

struct ChunkMetadata
{
	void* m_chunkOwner;
	int64_t m_markValue;
	size_t m_size;
	size_t m_allocThread;
};
template <typename T>
struct ChunkSlot
{

public:
	ChunkSlot() = default;
	~ChunkSlot() = default;
	ChunkMetadata m_metaData;
	T m_data;
};


template <typename T, int32_t SLOT_COUNT = CalcSlotCount<T>()>
class ChunkPage
{
public:
	ChunkPage();
	~ChunkPage() = default;
public:
	T* Alloc(size_t size = 0);
	void Free(void* data);

	void SetOwner(ObjectPool<T>* center);

	T* PopFromFreshSlot(size_t size);
	T* PopFromRecyledSlots(size_t size);

	PointerStack<ChunkSlot<T>*, SLOT_COUNT>* GetRecycledSlots();
public:
	ObjectPool<T>* m_owner;
	PointerStack<ChunkSlot<T>*, SLOT_COUNT> m_freshSlots;
	PointerStack<ChunkSlot<T>*, SLOT_COUNT>* m_recycledSlots{nullptr};
	FreeList<PointerStack<ChunkSlot<T>*, SLOT_COUNT>> m_recycledStackPool;

	ChunkSlot<T> m_data[SLOT_COUNT];
	static thread_local size_t m_cachedThreadID;
};

template<typename T, int SLOT_COUNT>
thread_local size_t ChunkPage<T, SLOT_COUNT>::m_cachedThreadID = std::hash<std::thread::id>{}(std::this_thread::get_id());

template <typename T, int32_t DEFAULT_SIZE>
ChunkPage<T, DEFAULT_SIZE>::ChunkPage()
{
	if (m_freshSlots.Empty())
	{
		for (size_t i = 0; i < DEFAULT_SIZE; ++i)
		{
			m_data[i].m_metaData.m_allocThread = m_cachedThreadID;
			m_data[i].m_metaData.m_chunkOwner = this;
			m_data[i].m_metaData.m_markValue = MARK_FRONT;
			m_freshSlots.Push(&m_data[i]);
		}
	}

	m_recycledSlots = m_recycledStackPool.Alloc();
}
template <typename T, int32_t SLOT_COUNT>
 T* ChunkPage<T, SLOT_COUNT>::Alloc(size_t size)
{
	if(auto rtnData = PopFromFreshSlot(size); rtnData != nullptr)
	{
		return rtnData;
	}

	if (auto rtnData = PopFromRecyledSlots(size); rtnData != nullptr)
	{
		return rtnData;
	}

	if (m_owner->TryStealFromGlobal(m_freshSlots))
	{
		if (auto rtnData = PopFromFreshSlot(size); rtnData != nullptr)
		{
			return rtnData;
		}
	}

	return m_owner->ChunkAlloc()->Alloc(size);
}

 template <typename T, int32_t SLOT_COUNT>
void ChunkPage<T, SLOT_COUNT>::Free(void* data)
{
	ChunkSlot<T>* dataPtr = reinterpret_cast<ChunkSlot<T>*>(reinterpret_cast<char*>(data) - offsetof(ChunkSlot<T>, m_data));
	//----------------------------------------------------
	// 반납된 포인터가 언더플로우 한 경우
	//----------------------------------------------------
	if (dataPtr->m_metaData.m_markValue != MARK_FRONT || dataPtr->m_metaData.m_chunkOwner != this)
	{
		throw(FreeListException(L"Underflow Violation", __LINE__));
	}

	if (dataPtr->m_metaData.m_allocThread == m_cachedThreadID)
	{
		m_freshSlots.Push(dataPtr);
	}
	else
	{
		m_owner->RecycledToMyLocalPool(dataPtr);
	}
}

template <typename T, int32_t SLOT_COUNT>
void ChunkPage<T, SLOT_COUNT>::SetOwner(ObjectPool<T>* center)
{
	m_owner = center;
}

template <typename T, int32_t SLOT_COUNT>
T* ChunkPage<T, SLOT_COUNT>::PopFromFreshSlot(size_t size)
{
	if(m_freshSlots.Empty())
	{
		return nullptr;
	}
	auto chunkHeader = m_freshSlots.Pop();
	chunkHeader->m_metaData.m_size = size;
	chunkHeader->m_metaData.m_allocThread = m_cachedThreadID;
	return &(chunkHeader->m_data);
}

template <typename T, int32_t SLOT_COUNT>
T* ChunkPage<T, SLOT_COUNT>::PopFromRecyledSlots(size_t size)
{
	if (m_recycledSlots->Empty())
	{
		return nullptr;
	}

	auto chunkHeader = m_recycledSlots->Pop();
	chunkHeader->m_metaData.m_size = size;
	chunkHeader->m_metaData.m_allocThread = m_cachedThreadID;
	return &(chunkHeader->m_data);
}
