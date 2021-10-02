#pragma once
#define ENQ_MODE 2
#define KERNEL_ADDRESS 0x80000000000
#include <iostream>
#include "FreeList.h"
#include "MemoryLog.h"

extern MemoryLogging_New<Q_LOG,10000> g_MemoryLogQ;
extern int64_t g_MemoryCount;
extern void Crash();

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
#if ENQ_MODE ==1
		m_RearID = 0;
		m_FrontCheck = (QCheck*)_aligned_malloc(sizeof(QCheck), 16);
		m_RearCheck = (QCheck*)_aligned_malloc(sizeof(QCheck), 16);
		//-----------------------------------------------
		// 더미노드 생성
		// Front ->  Dummy Node  <- Rear
		//-----------------------------------------------

		m_FrontCheck->_NodePtr = m_MemoryPool.Alloc();
		m_FrontCheck->_ID = 0;
		m_FrontCheck->_NodePtr->_Next = nullptr;

		m_RearCheck->_NodePtr = m_FrontCheck->_NodePtr;
		m_RearCheck->_ID = m_RearID;
		//m_RearCheck->_NodePtr->_Next = (Node*)m_RearCheck->_ID;
#endif
#if ENQ_MODE == 2
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
#endif
#if ENQ_MODE ==3
		m_RearID = 0;
		m_FrontCheck = (QCheck*)_aligned_malloc(sizeof(QCheck), 16);
		m_RearCheck = (QCheck*)_aligned_malloc(sizeof(QCheck), 16);
		//-----------------------------------------------
		// 더미노드 생성
		// Front ->  Dummy Node  <- Rear
		//-----------------------------------------------

		m_FrontCheck->_NodePtr = m_MemoryPool.Alloc();
		m_FrontCheck->_ID = 0;
		m_FrontCheck->_NodePtr->_Next = nullptr;

		m_RearCheck->_NodePtr = m_FrontCheck->_NodePtr;
		m_RearCheck->_ID = m_RearID;
		//m_RearCheck->_NodePtr->_Next = (Node*)m_RearCheck->_ID;
#endif

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

#if ENQ_MODE ==3
template<typename T>
inline bool LockFreeQ<T>::EnQ(T data)
{
	Q_LOG logData;
	int loopCount = 0;

	bool bCas1Fail = false;

	logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::ENTRY_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, -1, -1, (int64_t)data, -1, -1, m_Count);
	g_MemoryLogQ.MemoryLogging(logData);

	QCheck tempRear;
	QCheck changeValue;

	Node* newNode = m_MemoryPool.Alloc();

	//if (newNode == m_RearCheck->_NodePtr)
	//{
	//	Crash();
	//}
	newNode->_Data = data;
	newNode->_Next = nullptr;


	logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::ALLOC_NODE_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, -1, (int64_t)newNode, (int64_t)data, -1, -1, (int64_t)newNode->_Next, m_Count);
	g_MemoryLogQ.MemoryLogging(logData);

	do
	{
		bCas1Fail = false;
		loopCount++;
		tempRear._NodePtr = m_RearCheck->_NodePtr;
		tempRear._ID = m_RearCheck->_ID;


		logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::TEMP_REAR_SETTING_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, -1, -1, (int64_t)newNode->_Next,m_Count, loopCount);
		g_MemoryLogQ.MemoryLogging(logData);

		if (tempRear._NodePtr->_Next == tempRear._NodePtr)
		{
			Crash();
		}
		//--------------------------------------------------------------
		// Commit 1
		//--------------------------------------------------------------
		if ((int64_t)nullptr == InterlockedCompareExchange64((int64_t*)&tempRear._NodePtr->_Next, (int64_t)newNode, (int64_t)nullptr))
		{
			logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::CAS1_SUC_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)m_RearCheck->_NodePtr->_Next, -1, (int64_t)newNode->_Next, m_Count);
			g_MemoryLogQ.MemoryLogging(logData);
		}
		else
		{
			bCas1Fail = true;
		}
		
		while (true)
		{
			changeValue._NodePtr = tempRear._NodePtr->_Next;

			if (changeValue._NodePtr == nullptr)
			{
				break;
			}

			changeValue._ID = InterlockedIncrement64(&m_RearID);

			BOOL result = InterlockedCompareExchange128((LONG64*)m_RearCheck, (LONG64)changeValue._ID, (LONG64)changeValue._NodePtr, (LONG64*)&tempRear);
			if (result == TRUE)
			{
				logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::CAS1_FAIL_BUT_CHANGE_REAR_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)changeValue._NodePtr, -1, (int64_t)changeValue._NodePtr, m_Count);
				g_MemoryLogQ.MemoryLogging(logData);
				break;
			}
			else
			{
				logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::CAS1_FAIL_NO_CHANGE, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)changeValue._NodePtr, -1, (int64_t)changeValue._NodePtr, m_Count);
				g_MemoryLogQ.MemoryLogging(logData);

				//---------------------------------
				// 만일 CAS2 가 실패햇는데 , chagnaValue 가 newNode면  CAS1은 성공한거니
				// 재시도해보자
				//---------------------------------
				if (changeValue._NodePtr == newNode)
				{
					tempRear._NodePtr = m_RearCheck->_NodePtr;
					tempRear._ID = m_RearCheck->_ID;
					continue;
				}
			}	
		}
		
		if (bCas1Fail)
		{
			continue;
		}
		
		

			//while (true)
			//{
			//	changeValue._NodePtr = newNode;
			//	changeValue._ID = InterlockedIncrement64(&m_RearID);

			//	BOOL result = InterlockedCompareExchange128((LONG64*)m_RearCheck, (LONG64)changeValue._ID, (LONG64)changeValue._NodePtr, (LONG64*)&tempRear);
			//	//------------------------------------------------------
			//	// 여기서 판단되는거는 CAS1을 성공하지못한 다른 스레드가 m_Rear를 바꿧을때와
			//	// ABA로 들어온 옛날 m_Rear을 판단할 수 있다
			//	//------------------------------------------------------
			//	if (result == TRUE)
			//	{
			//		logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::CAS2_SUC_NEWNODE1_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)m_RearCheck->_NodePtr->_Next, -1, (int64_t)newNode->_Next, m_Count);
			//		g_MemoryLogQ.MemoryLogging(logData);
			//		break;
			//	}
			//	else
			//	{
			//		logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::CAS2_FAIL_NEWNODE1_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)m_RearCheck->_NodePtr->_Next, -1, (int64_t)newNode->_Next, m_Count);
			//		g_MemoryLogQ.MemoryLogging(logData);
			//	}

			//	tempRear._NodePtr = m_RearCheck->_NodePtr;
			//	tempRear._ID = m_RearCheck->_ID;

			//}
			//
			break;
		//}
	

	
	} while (true);
	InterlockedIncrement(&m_Count);
	InterlockedIncrement(&m_EnQTPS);

	logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::LOOP_OUT_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)m_RearCheck->_NodePtr->_Next, -1, (int64_t)newNode->_Next, m_Count);
	g_MemoryLogQ.MemoryLogging(logData);

	return true;
}

#endif

#if ENQ_MODE ==2
template<typename T>
inline bool LockFreeQ<T>::EnQ(T data)
{
	if (m_MaxQCount < m_Count)
	{
		Crash();
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
		//------------------------------------------------------
		// 여기서 판단되는거는 CAS1을 성공하지못한 다른 스레드가 m_Rear를 바꿧을때와
		// ABA로 들어온 옛날 m_Rear을 판단할 수 있다
		//------------------------------------------------------
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
#endif


#if ENQ_MODE ==1
template<typename T>
inline bool LockFreeQ<T>::EnQ(T data)
{

	Q_LOG logData;
	logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::ENTRY_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, -1, -1, (int64_t)data, -1, -1, m_Count);
	g_MemoryLogQ.MemoryLogging(logData);

	QCheck tempRear;

	Node* newNode = m_MemoryPool.Alloc();
	
	newNode->_Data = data;
	newNode->_Next = newNode;

	logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::ALLOC_NODE_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, -1, (int64_t)newNode, (int64_t)data, (int64_t)m_RearCheck->_NodePtr->_Next, -1, m_Count);
	g_MemoryLogQ.MemoryLogging(logData);

	do
	{
		tempRear._NodePtr = m_RearCheck->_NodePtr;
		tempRear._ID = m_RearCheck->_ID;

		if (tempRear._NodePtr == newNode)
		{
			logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::TEMPREAR_NEWNODE_SAME, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)m_RearCheck->_NodePtr->_Next, -1, m_Count);
			g_MemoryLogQ.MemoryLogging(logData);
			Crash();
		}

		logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::TEMP_REAR_SETTING_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)m_RearCheck->_NodePtr->_Next, -1, m_Count);
		g_MemoryLogQ.MemoryLogging(logData);

		//--------------------------------------------------------------
		// Commit 1
		//--------------------------------------------------------------
		if ((int64_t)nullptr != InterlockedCompareExchange64((int64_t*)&(tempRear._NodePtr->_Next), (int64_t)newNode, (int64_t)nullptr))
		{
			logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::CAS1_FAIL_NO_CHANGE, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)m_RearCheck->_NodePtr->_Next, -1, m_Count);
			g_MemoryLogQ.MemoryLogging(logData);
			continue;
		}
		else
		{
			logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::CAS1_SUC_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)m_RearCheck->_NodePtr->_Next, -1, m_Count);
			g_MemoryLogQ.MemoryLogging(logData);
		}

		m_RearCheck->_NodePtr = newNode;
		newNode->_Next = nullptr;

		logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::CAS2_SUC_NEWNODE1_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)m_RearCheck->_NodePtr->_Next, -1, (int64_t)newNode->_Next, m_Count);
		g_MemoryLogQ.MemoryLogging(logData);

		break;


	} while (true);

	InterlockedIncrement(&m_Count);
	InterlockedIncrement(&m_EnQTPS);

	logData.DataSettiong(InterlockedIncrement64(&g_MemoryCount), ePOS::LOOP_OUT_ENQ, GetCurrentThreadId(), (int64_t)m_FrontCheck->_NodePtr, (int64_t)m_RearCheck->_NodePtr, -1, (int64_t)tempRear._NodePtr, (int64_t)newNode, (int64_t)data, (int64_t)m_RearCheck->_NodePtr->_Next, -1, (int64_t)newNode->_Next, m_Count);
	g_MemoryLogQ.MemoryLogging(logData);
	

	return true;
}
#endif
template<typename T>
inline bool LockFreeQ<T>::DeQ(T* data)
{
	QCheck tempFront;
	QCheck changeValue;
	T tempData = NULL;


	if (InterlockedDecrement(&m_Count) < 0)
	{
		InterlockedIncrement(&m_Count);
		//Crash();
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
		//--------------------------------------------------------

#if ENQ_MODE ==2
		if ((int64_t)changeValue._NodePtr >= KERNEL_ADDRESS)
		{
			continue;
		}
#endif
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


