#pragma once

class FreeListException;
static int64_t g_FreeListUID = 0x0000000000000001;
struct MemHeader
{
	int64_t _MarkID;
	int64_t _MarkValue;
	std::atomic<bool> _FreeFlag{ false };
	size_t _Size;
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
		Node():_Next(nullptr){}
		T _Data;
		Node* _Next;
	};

	struct TopCheck
	{
		Node* _TopPtr;
		int64_t _ID;
	};
	struct AllocMemory
	{
		MemHeader _FrontMark;
		Node _Node;
	};

public:
	FreeList()
		:
		m_UseCount(0),
		m_PoolCount(0),
		m_AllocCount(0),
		m_FreeListUID(++g_FreeListUID)
	{
		m_TopCheck = static_cast<TopCheck*>(_aligned_malloc(sizeof(TopCheck), 16));
		m_TopCheck->_TopPtr = nullptr;
		m_TopCheck->_ID = 0;

	}
	FreeList(int32_t blockNum)
		:
		m_FreeListUID(++g_FreeListUID),
		m_UseCount(0),
		m_PoolCount(0),
		m_AllocCount(0)
	{
		m_TopCheck = static_cast<TopCheck*>(_aligned_malloc(sizeof(TopCheck), 16));
		m_TopCheck->_TopPtr = nullptr;
		m_TopCheck->_ID = 0;

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
		Node* curNode = m_TopCheck->_TopPtr;

		while (curNode != nullptr)
		{
			char* delNode = reinterpret_cast<char*>(curNode);

			//---------------------------------------------------------------------------------
			// 맨처음 메모리 malloc으로 할당할때, 객체화를 위해 해준, PlacementNew로 인한 생성자 호출때문에,
			// 소멸자를 맨마지막에는 호출해준다.
			//---------------------------------------------------------------------------------
			std::destroy_at(delNode);
			delNode = delNode - (sizeof(int64_t)*2);
			curNode = curNode->_Next;
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
	const int64_t m_FreeListUID;

	LONG m_PoolCount;
	LONG m_UseCount;
	LONG m_AllocCount;
};

template <typename T>
int64_t FreeList<T>::GetFreeListUID()
{
	return m_FreeListUID;
}

template<typename T>
inline int32_t FreeList<T>::GetPoolCount()
{
	return m_PoolCount;
}

template<typename T>
inline int32_t FreeList<T>::GetUseCount()
{
	return m_UseCount;
}

template<typename T>
inline int32_t FreeList<T>::GetAllocCount()
{
	return m_AllocCount;
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
	allocMemory->_FrontMark._FreeFlag = false;
	allocMemory->_FrontMark._Size = size;
	allocMemory->_FrontMark._MarkID = m_FreeListUID;
	allocMemory->_FrontMark._MarkValue = MARK_FRONT;

	//생성자 호출해줘야함 (제거금지)
	std::construct_at(&allocMemory->_Node);
	return reinterpret_cast<T*>(&allocMemory->_Node);
}

template<typename T>
bool FreeList<T>::Free(T* data)
{
	Node* freeNode = reinterpret_cast<Node*>(data);
	AllocMemory* allocMemory = reinterpret_cast<AllocMemory*>(reinterpret_cast<char*>(data) - offsetof(AllocMemory,_Node));

	//----------------------------------------------------
	// 반납된 포인터가 언더플로우 한 경우
	//----------------------------------------------------
	if (allocMemory->_FrontMark._MarkID != m_FreeListUID || allocMemory->_FrontMark._MarkValue != MARK_FRONT)
	{
		throw(FreeListException(L"Underflow Violation", __LINE__));
		return false;
	}
	//----------------------------------------------------
	// 두번 반납된 경우
	//----------------------------------------------------
	if (allocMemory->_FrontMark._FreeFlag.exchange(true) == true)
	{
		throw(FreeListException(L"Twice Free", __LINE__));
		return false;
	}

	TopCheck tempTop;
	tempTop._TopPtr = m_TopCheck->_TopPtr;
	tempTop._ID = m_TopCheck->_ID;
	int64_t spinCount = 0;
	do
	{
		//-------------------------------------------------------------------------------------------
		//  FreeNode(반납된 노드) ->  CurrentTop  반납된 노드의 Next포인터를 현재의 Top을가르키게함
		//-------------------------------------------------------------------------------------------
		freeNode->_Next = tempTop._TopPtr;

		auto result = InterlockedCompareExchange128(reinterpret_cast<int64_t*>(m_TopCheck), static_cast <int64_t>(tempTop._ID) + 1, reinterpret_cast<int64_t>(freeNode), reinterpret_cast<int64_t*>(&tempTop));
		if(result == TRUE)
		{
			break;
		}
		else
		{
			++spinCount;
			YieldProcessor();
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

	tempTop._TopPtr = m_TopCheck->_TopPtr;
	tempTop._ID = m_TopCheck->_ID;
	int64_t spinCount = 0;
	do
	{
		if (tempTop._TopPtr == nullptr)
		{
			return AllocateMemoryFromHeap(size);
		}
		auto result = InterlockedCompareExchange128(reinterpret_cast<LONG64*>(m_TopCheck), static_cast<LONG64>(tempTop._ID) + 1, reinterpret_cast<LONG64>(tempTop._TopPtr->_Next), reinterpret_cast<LONG64*>(&tempTop));
		if (result == TRUE)
		{
			break;
		}
		else
		{
			++spinCount;
			YieldProcessor();
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
	auto allocMemory = reinterpret_cast<AllocMemory*>(reinterpret_cast<char*>(tempTop._TopPtr) - offsetof(AllocMemory, _Node));
	allocMemory->_FrontMark._FreeFlag.exchange(false);

	//InterlockedIncrement(&m_UseCount);
	//InterlockedDecrement(&m_PoolCount);

	Node* rtnNode = tempTop._TopPtr;
	std::construct_at(rtnNode);

	return &rtnNode->_Data;
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
