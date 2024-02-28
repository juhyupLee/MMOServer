#pragma once
#include "FreeList.h"
#define KERNEL_ADDRESS 0x80000000000

template <typename T>
class LockFreeQ
{
	struct Node
	{
		Node()
		{
			_Data = NULL;
			_Next = nullptr;
		}
		T _Data;
		Node* _Next;

	};

	struct QCheck
	{
		Node* _NodePtr;
		int64_t _ID;
	};

public:
	LockFreeQ(DWORD maxQCount=INT32_MAX)
	{
		m_MaxQCount = maxQCount;

		m_RearID = KERNEL_ADDRESS;
		m_Count = 0;
		m_FrontCheck = (QCheck*)_aligned_malloc(sizeof(QCheck), 16);
		m_RearCheck = (QCheck*)_aligned_malloc(sizeof(QCheck), 16);

		//-----------------------------------------------
		// 더미노드 생성
		// Front ->  Dummy Node  <- Rear
		//-----------------------------------------------
		m_FrontCheck->_NodePtr = m_MemoryPool.Alloc();
		m_FrontCheck->_ID = 0;
		m_FrontCheck->_NodePtr->_Next = (Node*)m_RearID;

		m_RearCheck->_NodePtr = m_FrontCheck->_NodePtr;
		m_RearCheck->_ID = m_RearID;
		//m_RearCheck->_NodePtr->_Next = (Node*)m_RearCheck->_ID;

		m_DeQTPS = 0;
		m_EnQTPS = 0;

	}
	~LockFreeQ()
	{
		
		Node* curNode = m_FrontCheck->_NodePtr;
		Node* delNode = nullptr;

		while ((int64_t)curNode < KERNEL_ADDRESS)
		{
			delNode = curNode;
			curNode = curNode->_Next;
			m_MemoryPool.Free(delNode);
		}

		_aligned_free(m_FrontCheck);
		_aligned_free(m_RearCheck);

	}
	LONG GetQCount()
	{
		return m_Count;
	}
	int32_t  GetMemoryPoolAllocCount()
	{
		return m_MemoryPool.GetAllocCount();
	}
	bool EnQ(T data);
	bool DeQ(T* data);


public:
	LONG m_Count;

public:
	LONG m_DeQTPS;
	LONG m_EnQTPS;

	QCheck* m_FrontCheck;
	QCheck* m_RearCheck;
	int64_t m_RearID;

	FreeList<Node> m_MemoryPool;
	LONG m_MaxQCount;

};


template<typename T>
inline bool LockFreeQ<T>::EnQ(T data)
{
	if (m_MaxQCount < m_Count)
	{
		CRASH();
		return false;
	}

	QCheck tempRear;
	QCheck changeValue;
	Node* newNode = m_MemoryPool.Alloc();

	newNode->_Data = data;
	newNode->_Next = (Node*)InterlockedIncrement64(&m_RearID);

	do
	{

		tempRear._NodePtr = m_RearCheck->_NodePtr;
		tempRear._ID = m_RearCheck->_ID;
		//-------------------------------------------------------------- 
		// CAS(Compare And Swap) 1 
		//-------------------------------------------------------------- 
		if ((int64_t)tempRear._ID != InterlockedCompareExchange64((int64_t*)&tempRear._NodePtr->_Next, (int64_t)newNode, (int64_t)tempRear._ID))
		{
			//-------------------------------------------------------------- 
			// CAS1에 실패했을경우 
			// Rear Pointer라도 갱신시켜준다. 그리고 다시 CAS1을 시도한다 
			// 그런데 만약 Temp Rear의 Nert포인터가 커널영역의 가상메모리 주소라면 
			// 이미 갱신까지 마친것으로, 다시 처음부터 CAS1을 시도한다. 
			//-------------------------------------------------------------- 
			changeValue._NodePtr = tempRear._NodePtr->_Next;

			if ((int64_t)changeValue._NodePtr >= KERNEL_ADDRESS)
			{
				continue;
			}

			changeValue._ID = (int64_t)changeValue._NodePtr->_Next;

			BOOL result = InterlockedCompareExchange128((LONG64*)m_RearCheck, (LONG64)changeValue._ID, (LONG64)changeValue._NodePtr, (LONG64*)&tempRear);

			continue;

		}

		//-------------------------------------------------------------- 
		// CAS1에 성공한 경우 CAS2를 진행한다. 
		// CAS2에 실패하더라도 이미 CAS1은 성공했으므로 사실상 Enqueue의 성공한것으로봄 
		// 그래서 CAS2의 성공 실패를 따지지 않고 Loop를 종료한다 
		// CAS2는 Rear의 포인터와 현재 이 EnQ ID를 갱신시켜준다. 
		//-------------------------------------------------------------- 
		changeValue._NodePtr = newNode;
		changeValue._ID = (int64_t)newNode->_Next;

		//-------------------------------------------------------------- 
		// CAS(Compare And Swap) 2 
		//-------------------------------------------------------------- 
		BOOL result = InterlockedCompareExchange128((LONG64*)m_RearCheck, (LONG64)changeValue._ID, (LONG64)changeValue._NodePtr, (LONG64*)&tempRear);
		break;

	} while (true);

	InterlockedIncrement(&m_Count);

	return true;
}


template<typename T>
inline bool LockFreeQ<T>::DeQ(T* data)
{
	QCheck tempFront;
	QCheck changeValue;
	T tempData = NULL;


	if (InterlockedDecrement(&m_Count) < 0)
	{
		InterlockedIncrement(&m_Count);
		return false;
	}

	tempFront._NodePtr = m_FrontCheck->_NodePtr;
	tempFront._ID = m_FrontCheck->_ID;

	do
	{
		//-----------------------------
		// Change Value Setting
		//-----------------------------
		changeValue._NodePtr = tempFront._NodePtr->_Next;

		//--------------------------------------------------------
		// 동시에 tempRear을 같은것을 가르킨다거나, ABA문제가 나타나면 
		// 아래와같은 Next->끝인 상황이 나올 수 있다 
		// 근데 내경우는 ABA문제를 해결하는 코드이기때문에 전자일것이다.
		//-------------------------------------------------------

		if ((int64_t)changeValue._NodePtr >= KERNEL_ADDRESS)
		{
			continue;
		}

		changeValue._ID = tempFront._ID + 1;
		//--------------------------------------------
		// tempData를 미리 저장해놔야한다.
		//--------------------------------------------
		tempData = changeValue._NodePtr->_Data;

		BOOL result = InterlockedCompareExchange128((LONG64*)m_FrontCheck, (LONG64)changeValue._ID, (LONG64)changeValue._NodePtr, (LONG64*)&tempFront);
		if (result == TRUE)
		{
			break;
		}
		else
		{
			continue;
		}
	} while (true);

	InterlockedIncrement(&m_DeQTPS);

	m_MemoryPool.Free(tempFront._NodePtr);

	*data = tempData;

	return true;
}


