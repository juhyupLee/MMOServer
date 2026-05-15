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
	LockFreeQ(uint32_t maxQCount = INT32_MAX)
	{
		m_maxQCount = static_cast<int32_t>(maxQCount);

		m_rearID = KERNEL_ADDRESS;
		m_count = 0;
		m_frontCheck = (QCheck*)AlignedMalloc(sizeof(QCheck), 16);
		m_rearCheck = (QCheck*)AlignedMalloc(sizeof(QCheck), 16);

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

		AlignedFree(m_frontCheck);
		AlignedFree(m_rearCheck);

	}
	int32_t GetQCount()
	{
		return m_count.load();
	}
	int32_t  GetMemoryPoolAllocCount()
	{
		return m_memoryPool.GetAllocCount();
	}
	bool EnQ(T data);
	bool DeQ(T* data);


public:
	std::atomic<int32_t> m_count;

public:
	std::atomic<int32_t> m_deQTPS;
	std::atomic<int32_t> m_enQTPS;

	QCheck* m_frontCheck;
	QCheck* m_rearCheck;
	int64_t m_rearID;

	FreeList<Node> m_memoryPool;
	int32_t m_maxQCount;

};


template<typename T>
inline bool LockFreeQ<T>::EnQ(T data)
{
	if (m_maxQCount < m_count.load())
	{
		CRASH();
		return false;
	}

	QCheck tempRear;
	QCheck changeValue;
	Node* newNode = m_memoryPool.Alloc();

	newNode->m_data = data;
	newNode->m_next = (Node*)AtomicIncrement64(&m_rearID);

	do
	{

		tempRear.m_nodePtr = m_rearCheck->m_nodePtr;
		tempRear.m_checkID = m_rearCheck->m_checkID;
		//--------------------------------------------------------------
		// CAS(Compare And Swap) 1
		//--------------------------------------------------------------
		if ((int64_t)tempRear.m_checkID != AtomicCAS64((int64_t*)&tempRear.m_nodePtr->m_next, (int64_t)newNode, (int64_t)tempRear.m_checkID))
		{
			//--------------------------------------------------------------
			// CAS1에 실패했을경우
			//--------------------------------------------------------------
			changeValue.m_nodePtr = tempRear.m_nodePtr->m_next;

			if ((int64_t)changeValue.m_nodePtr >= KERNEL_ADDRESS)
			{
				continue;
			}

			changeValue.m_checkID = (int64_t)changeValue.m_nodePtr->m_next;

			AtomicCompareExchange128((int64_t*)m_rearCheck, (int64_t)changeValue.m_checkID, (int64_t)changeValue.m_nodePtr, (int64_t*)&tempRear);

			continue;

		}

		//--------------------------------------------------------------
		// CAS1에 성공한 경우 CAS2를 진행한다.
		//--------------------------------------------------------------
		changeValue.m_nodePtr = newNode;
		changeValue.m_checkID = (int64_t)newNode->m_next;

		//--------------------------------------------------------------
		// CAS(Compare And Swap) 2
		//--------------------------------------------------------------
		AtomicCompareExchange128((int64_t*)m_rearCheck, (int64_t)changeValue.m_checkID, (int64_t)changeValue.m_nodePtr, (int64_t*)&tempRear);
		break;

	} while (true);

	m_count.fetch_add(1);

	return true;
}


template<typename T>
inline bool LockFreeQ<T>::DeQ(T* data)
{
	QCheck tempFront;
	QCheck changeValue;
	T tempData = NULL;


	if (m_count.fetch_sub(1) < 1)
	{
		m_count.fetch_add(1);
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

		if ((int64_t)changeValue.m_nodePtr >= KERNEL_ADDRESS)
		{
			continue;
		}

		changeValue.m_checkID = tempFront.m_checkID + 1;
		//--------------------------------------------
		// tempData를 미리 저장해놔야한다.
		//--------------------------------------------
		tempData = changeValue.m_nodePtr->m_data;

		bool result = AtomicCompareExchange128((int64_t*)m_frontCheck, (int64_t)changeValue.m_checkID, (int64_t)changeValue.m_nodePtr, (int64_t*)&tempFront);
		if (result)
		{
			break;
		}
		else
		{
			continue;
		}
	} while (true);

	m_deQTPS.fetch_add(1);

	m_memoryPool.Free(tempFront.m_nodePtr);

	*data = tempData;

	return true;
}

