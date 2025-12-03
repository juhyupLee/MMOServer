#pragma once
#define KERNEL_ADDRESS 0x80000000000

template <typename T>
class LockFreeQ
{
	struct Node
	{
		Node()
		{
			m_data = NULL;
			m_next = nullptr;
		}
		T m_data;
		Node* m_next;

	};

	struct QCheck
	{
		Node* m_nodePtr;
		int64_t m_checkID;
	};

public:
	LockFreeQ(DWORD maxQCount = INT32_MAX)
	{
		m_maxQCount = maxQCount;

		m_rearID = KERNEL_ADDRESS;
		m_count = 0;
		m_frontCheck = (QCheck*)_aligned_malloc(sizeof(QCheck), 16);
		m_rearCheck = (QCheck*)_aligned_malloc(sizeof(QCheck), 16);

		//-----------------------------------------------
		// 더미노드 생성
		// Front ->  Dummy Node  <- Rear
		//-----------------------------------------------
		m_frontCheck->m_nodePtr = m_memoryPool.Alloc();
		m_frontCheck->m_checkID = 0;
		m_frontCheck->m_nodePtr->m_next = (Node*)m_rearID;

		m_rearCheck->m_nodePtr = m_frontCheck->m_nodePtr;
		m_rearCheck->m_checkID = m_rearID;

		m_deQTPS = 0;
		m_enQTPS = 0;

	}
	~LockFreeQ()
	{
		
		Node* curNode = m_frontCheck->m_nodePtr;
		Node* delNode = nullptr;

		while ((int64_t)curNode < KERNEL_ADDRESS)
		{
			delNode = curNode;
			curNode = curNode->m_next;
			m_memoryPool.Free(delNode);
		}

		_aligned_free(m_frontCheck);
		_aligned_free(m_rearCheck);

	}
	LONG GetQCount()
	{
		return m_count;
	}
	int32_t  GetMemoryPoolAllocCount()
	{
		return m_memoryPool.GetAllocCount();
	}
	bool EnQ(T data);
	bool DeQ(T* data);


public:
	LONG m_count;

public:
	LONG m_deQTPS;
	LONG m_enQTPS;

	QCheck* m_frontCheck;
	QCheck* m_rearCheck;
	int64_t m_rearID;

	FreeList<Node> m_memoryPool;
	LONG m_maxQCount;

};


template<typename T>
inline bool LockFreeQ<T>::EnQ(T data)
{
	if (m_maxQCount < m_count)
	{
		CRASH();
		return false;
	}

	QCheck tempRear;
	QCheck changeValue;
	Node* newNode = m_memoryPool.Alloc();

	newNode->m_data = data;
	newNode->m_next = (Node*)InterlockedIncrement64(&m_rearID);

	do
	{

		tempRear.m_nodePtr = m_rearCheck->m_nodePtr;
		tempRear.m_checkID = m_rearCheck->m_checkID;
		//-------------------------------------------------------------- 
		// CAS(Compare And Swap) 1 
		//-------------------------------------------------------------- 
		if ((int64_t)tempRear.m_checkID != InterlockedCompareExchange64((int64_t*)&tempRear.m_nodePtr->m_next, (int64_t)newNode, (int64_t)tempRear.m_checkID))
		{
			//-------------------------------------------------------------- 
			// CAS1에 실패했을경우 
			// Rear Pointer라도 갱신시켜준다. 그리고 다시 CAS1을 시도한다 
			// 그런데 만약 Temp Rear의 Nert포인터가 커널영역의 가상메모리 주소라면 
			// 이미 갱신까지 마친것으로, 다시 처음부터 CAS1을 시도한다. 
			//-------------------------------------------------------------- 
			changeValue.m_nodePtr = tempRear.m_nodePtr->m_next;

			if ((int64_t)changeValue.m_nodePtr >= KERNEL_ADDRESS)
			{
				continue;
			}

			changeValue.m_checkID = (int64_t)changeValue.m_nodePtr->m_next;

			BOOL result = InterlockedCompareExchange128((LONG64*)m_rearCheck, (LONG64)changeValue.m_checkID, (LONG64)changeValue.m_nodePtr, (LONG64*)&tempRear);

			continue;

		}

		//-------------------------------------------------------------- 
		// CAS1에 성공한 경우 CAS2를 진행한다. 
		// CAS2에 실패하더라도 이미 CAS1은 성공했으므로 사실상 Enqueue의 성공한것으로봄 
		// 그래서 CAS2의 성공 실패를 따지지 않고 Loop를 종료한다 
		// CAS2는 Rear의 포인터와 현재 이 EnQ ID를 갱신시켜준다. 
		//-------------------------------------------------------------- 
		changeValue.m_nodePtr = newNode;
		changeValue.m_checkID = (int64_t)newNode->m_next;

		//-------------------------------------------------------------- 
		// CAS(Compare And Swap) 2 
		//-------------------------------------------------------------- 
		BOOL result = InterlockedCompareExchange128((LONG64*)m_rearCheck, (LONG64)changeValue.m_checkID, (LONG64)changeValue.m_nodePtr, (LONG64*)&tempRear);
		break;

	} while (true);

	InterlockedIncrement(&m_count);

	return true;
}


template<typename T>
inline bool LockFreeQ<T>::DeQ(T* data)
{
	QCheck tempFront;
	QCheck changeValue;
	T tempData = NULL;


	if (InterlockedDecrement(&m_count) < 0)
	{
		InterlockedIncrement(&m_count);
		return false;
	}

	tempFront.m_nodePtr = m_frontCheck->m_nodePtr;
	tempFront.m_checkID = m_frontCheck->m_checkID;

	do
	{
		//-----------------------------
		// Change Value Setting
		//-----------------------------
		changeValue.m_nodePtr = tempFront.m_nodePtr->m_next;

		//--------------------------------------------------------
		// 동시에 tempRear을 같은것을 가르킨다거나, ABA문제가 나타나면 
		// 아래와같은 Next->끝인 상황이 나올 수 있다 
		// 근데 내경우는 ABA문제를 해결하는 코드이기때문에 전자일것이다.
		//-------------------------------------------------------

		if ((int64_t)changeValue.m_nodePtr >= KERNEL_ADDRESS)
		{
			continue;
		}

		changeValue.m_checkID = tempFront.m_checkID + 1;
		//--------------------------------------------
		// tempData를 미리 저장해놔야한다.
		//--------------------------------------------
		tempData = changeValue.m_nodePtr->m_data;

		BOOL result = InterlockedCompareExchange128((LONG64*)m_frontCheck, (LONG64)changeValue.m_checkID, (LONG64)changeValue.m_nodePtr, (LONG64*)&tempFront);
		if (result == TRUE)
		{
			break;
		}
		else
		{
			continue;
		}
	} while (true);

	InterlockedIncrement(&m_deQTPS);

	m_memoryPool.Free(tempFront.m_nodePtr);

	*data = tempData;

	return true;
}


