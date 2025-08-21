#pragma once
template <typename T>
class LockFreeStack
{
public:
	LockFreeStack();
	~LockFreeStack();
	struct Node
	{
		Node()
		{
			_Next = nullptr;
		}
		T _Data;
		Node* _Next;
	};

	struct TopCheck
	{
		Node* _TopPtr;
		int64_t _ID;
	};

public:
	bool Push(T data);
	bool Pop(T* outData);
	int32_t GetStackCount();

	//--------------------------------
	// For Debugging
	//--------------------------------

	int32_t GetMemoryAllocCount();
public:
	alignas(64) TopCheck* m_TopCheck;
	alignas(64) std::atomic<int32_t> m_Count{ 0 };

	FreeList<Node> m_MemoryPool;

};

template<typename T>
inline LockFreeStack<T>::LockFreeStack()
{
	m_Count =0;

	m_TopCheck = static_cast<TopCheck*>(_aligned_malloc(sizeof(TopCheck), 16));
	m_TopCheck->_TopPtr = nullptr;
	m_TopCheck->_ID = -1;

}

template<typename T>
inline LockFreeStack<T>::~LockFreeStack()
{
	Node* curNode = m_TopCheck->_TopPtr;
	Node* delNode = nullptr;

	while (curNode != nullptr)
	{
		delNode = curNode;
		curNode = curNode->_Next;
		m_MemoryPool.Free(delNode);
	}
	_aligned_free(m_TopCheck);
}

template<typename T>
inline bool LockFreeStack<T>::Push(T data)
{
	Node* newNode = m_MemoryPool.Alloc();

	newNode->_Data = data;
	newNode->_Next = nullptr;

	TopCheck tempTop;
	
	tempTop._TopPtr = m_TopCheck->_TopPtr;
	tempTop._ID = m_TopCheck->_ID;
	int64_t spinCount = 0;
	do
	{
		newNode->_Next = tempTop._TopPtr;
		if(InterlockedCompareExchange128(reinterpret_cast<LONG64*>(m_TopCheck), static_cast<LONG64>(tempTop._ID) + 1, reinterpret_cast<LONG64>(newNode), reinterpret_cast<LONG64*>(&tempTop)))
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
		else if (spinCount >= MAX_SLEEP_ITERATION)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}

	} while (true);

	spinCount = 0;
	m_Count++;
	return true;
}

template<typename T>
inline bool LockFreeStack<T>::Pop(T* outData)
{
	if (--m_Count < 0)
	{
		++m_Count;
		return false;
	}

	TopCheck tempTop;

	tempTop._TopPtr = m_TopCheck->_TopPtr;
	tempTop._ID = m_TopCheck->_ID;
	int64_t spinCount = 0;
	do
	{
		if (InterlockedCompareExchange128(reinterpret_cast<LONG64*>(m_TopCheck), static_cast<LONG64>(tempTop._ID) + 1, reinterpret_cast<LONG64>(tempTop._TopPtr->_Next), reinterpret_cast<LONG64*>(&tempTop)))
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
		else if (spinCount >= MAX_SLEEP_ITERATION)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}

	} while (true);

	spinCount = 0;
	*outData = tempTop._TopPtr->_Data;
	m_MemoryPool.Free(tempTop._TopPtr);
	return true;
}


template<typename T>
inline int32_t LockFreeStack<T>::GetStackCount()
{
	return m_Count;
}

template<typename T>
inline int32_t LockFreeStack<T>::GetMemoryAllocCount()
{
	return m_MemoryPool.GetAllocCount();
}
