#pragma once

class FreeListException;
static int64_t g_FreeListUID = 0x0000000000000001;
struct MemHeader
{
	int64_t m_markID;
	int64_t m_markValue;
	std::atomic<bool> m_freeFlag{ false };
	size_t m_size;
};

class FreeListBase
{
public:
	FreeListBase() = default;
	virtual ~FreeListBase()=default;

	virtual void* AllocMem(size_t size) = 0;
	virtual bool FreeMem(void* ptr) = 0;
private:
	int32_t m_allocSize;
};
template <typename T>
class FreeList : public FreeListBase
{
private:
	struct Node
	{
		Node():m_next(nullptr){}
		T m_data;
		Node* m_next;
	};

	struct TopCheck
	{
		Node* m_topPtr;
		int64_t m_checkID;
	};
	struct AllocMemory
	{
		MemHeader m_frontMark;
		Node m_node;
	};

public:
	FreeList()
		:
		m_useCount(0),
		m_poolCount(0),
		m_allocCount(0),
		m_freeListUID(++g_FreeListUID)
	{
		m_TopCheck = static_cast<TopCheck*>(_aligned_malloc(sizeof(TopCheck), 16));
		m_TopCheck->m_topPtr = nullptr;
		m_TopCheck->m_checkID = 0;

	}
	FreeList(int32_t blockNum)
		:
		m_freeListUID(++g_FreeListUID),
		m_useCount(0),
		m_poolCount(0),
		m_allocCount(0)
	{
		m_TopCheck = static_cast<TopCheck*>(_aligned_malloc(sizeof(TopCheck), 16));
		m_TopCheck->m_topPtr = nullptr;
		m_TopCheck->m_checkID = 0;

		for (int i = 0; i < blockNum; i++)
		{
			T* data = AllocateMemoryFromHeap();
			Free(data);
		}
	}
	~FreeList()
	{
		//--------------------------------------------------------------------------
		// 더미노드를 제외한, 데이터 노드들은 기존 포인터에서 128바이트 만큼 땡겨, delete를 해야한다
		//--------------------------------------------------------------------------
		Node* curNode = m_TopCheck->m_topPtr;

		while (curNode != nullptr)
		{
			auto delNode = reinterpret_cast<AllocMemory*>(reinterpret_cast<char*>(curNode) - offsetof(AllocMemory, m_node));
			curNode = curNode->m_next;
			_aligned_free(delNode);
		}

		_aligned_free(m_TopCheck);
	}

	int64_t GetFreeListUID();
	int32_t GetPoolCount();
	int32_t GetUseCount();
	int32_t GetAllocCount();
	bool Free(T* data);
	T* Alloc(size_t size = 0);

	virtual void* AllocMem(size_t size) override;
	virtual bool FreeMem(void* ptr) override;

public:
	T* AllocateMemoryFromHeap(size_t size = 0);

private:

	TopCheck* m_TopCheck;
	const int64_t m_freeListUID;

	LONG m_poolCount;
	LONG m_useCount;
	LONG m_allocCount;
};

template <typename T>
int64_t FreeList<T>::GetFreeListUID()
{
	return m_freeListUID;
}

template<typename T>
inline int32_t FreeList<T>::GetPoolCount()
{
	return m_poolCount;
}

template<typename T>
inline int32_t FreeList<T>::GetUseCount()
{
	return m_useCount;
}

template<typename T>
inline int32_t FreeList<T>::GetAllocCount()
{
	return m_allocCount;
}

template <typename T>
bool FreeList<T>::FreeMem(void* ptr)
{
	return Free(reinterpret_cast<T*>(ptr));
}

template<typename T>
inline T* FreeList<T>::AllocateMemoryFromHeap(size_t size)
{
	//--------------------------------------------------------------------------
	// 언더플로우 체크용 mark ID  + data(Payload)  + 오버플로우 체크용 mark ID  할당
	//--------------------------------------------------------------------------
	AllocMemory* allocMemory = static_cast<AllocMemory*>(_aligned_malloc(sizeof(AllocMemory), 16));
	allocMemory->m_frontMark.m_freeFlag = false;
	allocMemory->m_frontMark.m_size = size;
	allocMemory->m_frontMark.m_markID = m_freeListUID;
	allocMemory->m_frontMark.m_markValue = MARK_FRONT;

	//생성자 호출해줘야함 (제거금지)
	std::construct_at(&allocMemory->m_node);
	return reinterpret_cast<T*>(&allocMemory->m_node);
}

template<typename T>
bool FreeList<T>::Free(T* data)
{
	Node* freeNode = reinterpret_cast<Node*>(data);
	AllocMemory* allocMemory = reinterpret_cast<AllocMemory*>(reinterpret_cast<char*>(data) - offsetof(AllocMemory,m_node));

	//----------------------------------------------------
	// 반납된 포인터가 언더플로우 한 경우
	//----------------------------------------------------
	if (allocMemory->m_frontMark.m_markID != m_freeListUID ||
		allocMemory->m_frontMark.m_markValue != MARK_FRONT)
	{
		throw(FreeListException(L"Underflow Violation", __LINE__));
	}
	//----------------------------------------------------
	// 두번 반납된 경우
	//----------------------------------------------------
	if (allocMemory->m_frontMark.m_freeFlag.exchange(true) == true)
	{
		throw(FreeListException(L"Twice Free", __LINE__));
	}

	TopCheck tempTop;
	tempTop.m_topPtr = m_TopCheck->m_topPtr;
	tempTop.m_checkID = m_TopCheck->m_checkID;
	int64_t spinCount = 0;
	do
	{
		//-------------------------------------------------------------------------------------------
		//  FreeNode(반납된 노드) ->  CurrentTop  반납된 노드의 Next포인터를 현재의 Top을가르키게함
		//-------------------------------------------------------------------------------------------
		freeNode->m_next = tempTop.m_topPtr;

		auto result = InterlockedCompareExchange128(reinterpret_cast<int64_t*>(m_TopCheck), static_cast <int64_t>(tempTop.m_checkID) + 1, reinterpret_cast<int64_t>(freeNode), reinterpret_cast<int64_t*>(&tempTop));
		if(result == TRUE)
		{
			break;
		}
		else
		{
			++spinCount;
			for (auto i = 0; i < 1024; ++i)
			{
				YieldProcessor();
			}
		}

		if(spinCount >= YIELD_TRY_MAX)
		{
			std::this_thread::yield();
		}
		else if (spinCount >= MAX_SLEEP_ITERATION)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}

	} while (true);

	spinCount = 0;
	return true;
}

template <typename T>
T* FreeList<T>::Alloc(size_t size)
{
	TopCheck tempTop;

	tempTop.m_topPtr = m_TopCheck->m_topPtr;
	tempTop.m_checkID = m_TopCheck->m_checkID;
	int64_t spinCount = 0;
	do
	{
		if (tempTop.m_topPtr == nullptr)
		{
			return AllocateMemoryFromHeap(size);
		}
		auto result = InterlockedCompareExchange128(reinterpret_cast<LONG64*>(m_TopCheck), static_cast<LONG64>(tempTop.m_checkID) + 1, reinterpret_cast<LONG64>(tempTop.m_topPtr->m_next), reinterpret_cast<LONG64*>(&tempTop));
		if (result == TRUE)
		{
			break;
		}
		else
		{
			++spinCount;
			for (auto i = 0; i < 1024; ++i)
			{
				YieldProcessor();
			}
		}

		if (spinCount >= YIELD_TRY_MAX)
		{
			std::this_thread::yield();
		}
		else if(spinCount >= MAX_SLEEP_ITERATION)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
		

	} while (true);

	spinCount = 0;
	auto allocMemory = reinterpret_cast<AllocMemory*>(reinterpret_cast<char*>(tempTop.m_topPtr) - offsetof(AllocMemory, m_node));
	allocMemory->m_frontMark.m_freeFlag.exchange(false);

	//InterlockedIncrement(&m_useCount);
	//InterlockedDecrement(&m_poolCount);

	Node* rtnNode = tempTop.m_topPtr;
	std::construct_at(rtnNode);

	return &rtnNode->m_data;
}

template <typename T>
void* FreeList<T>::AllocMem(size_t size)
{
	return  reinterpret_cast<void*>(Alloc(size));
}


class FreeListException
{
public:
	FreeListException(const wchar_t* str, int line)
		:m_Line(line)
	{
		wcscpy_s(m_String, str);
	}
	void what()
	{
		wprintf(L"%s  [File:FreeList.h] [Line:%d]\n", m_String, m_Line);
	}
	int m_Line;
	wchar_t m_String[128];
};
