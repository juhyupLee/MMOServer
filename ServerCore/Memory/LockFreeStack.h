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
			m_next = nullptr;
		}
		T m_data;
		Node* m_next;
	};

	struct TopCheck
	{
		Node* m_topPtr;
		int64_t m_checkID;
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
	alignas(64) TopCheck* m_topCheck;
	alignas(64) std::atomic<int32_t> m_count{ 0 };
	FreeList<Node> m_memoryPool;
};

template<typename T>
inline LockFreeStack<T>::LockFreeStack()
{
	m_count =0;

	m_topCheck = static_cast<TopCheck*>(_aligned_malloc(sizeof(TopCheck), 16));
	m_topCheck->m_topPtr = nullptr;
	m_topCheck->m_checkID = -1;

}

template<typename T>
inline LockFreeStack<T>::~LockFreeStack()
{
	Node* curNode = m_topCheck->m_topPtr;
	Node* delNode = nullptr;

	while (curNode != nullptr)
	{
		delNode = curNode;
		curNode = curNode->m_next;
		m_memoryPool.Free(delNode);
	}
	_aligned_free(m_topCheck);
}

template<typename T>
inline bool LockFreeStack<T>::Push(T data)
{
	Node* newNode = m_memoryPool.Alloc();

	newNode->m_data = data;
	newNode->m_next = nullptr;

	TopCheck tempTop;
	
	tempTop.m_topPtr = m_topCheck->m_topPtr;
	tempTop.m_checkID = m_topCheck->m_checkID;
	int64_t spinCount = 0;
	do
	{
		newNode->m_next = tempTop.m_topPtr;
		if(InterlockedCompareExchange128(reinterpret_cast<LONG64*>(m_topCheck), static_cast<LONG64>(tempTop.m_checkID) + 1, reinterpret_cast<LONG64>(newNode), reinterpret_cast<LONG64*>(&tempTop)))
		{
			break;
		}
		else
		{
			++spinCount;
			for(auto i = 0; i < 1024; ++i)
			{
				YieldProcessor();
			}
		}

		if (spinCount >= MAX_SLEEP_ITERATION)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
		else if (spinCount >= YIELD_TRY_MAX)
		{
			std::this_thread::yield();
		}

	} while (true);

	spinCount = 0;
	m_count++;
	return true;
}

template<typename T>
inline bool LockFreeStack<T>::Pop(T* outData)
{
	if (--m_count < 0)
	{
		++m_count;
		return false;
	}

	TopCheck tempTop;

	tempTop.m_topPtr = m_topCheck->m_topPtr;
	tempTop.m_checkID = m_topCheck->m_checkID;
	int64_t spinCount = 0;
	do
	{
		if (InterlockedCompareExchange128(reinterpret_cast<LONG64*>(m_topCheck), static_cast<LONG64>(tempTop.m_checkID) + 1, reinterpret_cast<LONG64>(tempTop.m_topPtr->m_next), reinterpret_cast<LONG64*>(&tempTop)))
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

		if (spinCount >= MAX_SLEEP_ITERATION)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
		else if (spinCount >= YIELD_TRY_MAX)
		{
			std::this_thread::yield();
		}

	} while (true);

	spinCount = 0;
	*outData = tempTop.m_topPtr->m_data;
	m_memoryPool.Free(tempTop.m_topPtr);
	return true;
}


template<typename T>
inline int32_t LockFreeStack<T>::GetStackCount()
{
	return m_count;
}

template<typename T>
inline int32_t LockFreeStack<T>::GetMemoryAllocCount()
{
	return m_memoryPool.GetAllocCount();
}
