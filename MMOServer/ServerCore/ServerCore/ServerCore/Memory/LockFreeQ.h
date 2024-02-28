#pragma once
#include "FreeList.h"
#define KERNEL_ADDRESS 0x80000000000

extern void CRASH();


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

	int loopCount = 0;

	QCheck tempRear;
	QCheck changeValue;

	Node* newNode = m_MemoryPool.Alloc();

	newNode->_Data = data;
	newNode->_Next = (Node*)InterlockedIncrement64(&m_RearID);

	do
	{
		loopCount++;
		tempRear._NodePtr = m_RearCheck->_NodePtr;
		tempRear._ID = m_RearCheck->_ID;


		//--------------------------------------------------------------
		// Commit 1
		//--------------------------------------------------------------
		if ((int64_t)tempRear._ID == InterlockedCompareExchange64((int64_t*)&tempRear._NodePtr->_Next, (int64_t)newNode, (int64_t)tempRear._ID))
		{

		}
		else
		{
			changeValue._NodePtr = tempRear._NodePtr->_Next;

			if ((int64_t)changeValue._NodePtr >= KERNEL_ADDRESS)
			{
				continue;
			}
			changeValue._ID = (int64_t)changeValue._NodePtr->_Next;

			BOOL result = InterlockedCompareExchange128((LONG64*)m_RearCheck, (LONG64)changeValue._ID, (LONG64)changeValue._NodePtr, (LONG64*)&tempRear);
			if (result == TRUE)
			{
			}
			else
			{

			}
			continue;
		}

		changeValue._NodePtr = newNode;
		changeValue._ID = (int64_t)newNode->_Next;


		BOOL result = InterlockedCompareExchange128((LONG64*)m_RearCheck, (LONG64)changeValue._ID, (LONG64)changeValue._NodePtr, (LONG64*)&tempRear);

			if (result == TRUE)
			{
			}
			else
			{
			}
		break;

	} while (true);

	InterlockedIncrement(&m_Count);
	InterlockedIncrement(&m_EnQTPS);

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


