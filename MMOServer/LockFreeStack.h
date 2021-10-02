#pragma once
#include <iostream>
#include <Windows.h>
#include "MemoryLog.h"
#include <unordered_map>
#include "FreeList.h"

#define NEWDELTE 0

#define TEST_COUNT 1000000

extern MyMemoryLog<int64_t> g_MemoryStackCount;
extern MyMemoryLog<int64_t,50000> g_MemoryStackPtr;
enum  eLine
{
	PUSH_ENTRY, //0
	NEWNODE_PUSH, //1
	TOP_COPY_PUSH, //2
	NEWNODE_NEXT_SETTING_PUSH,//3
	CHANGE_SETTING_PUSH,//4
	INTERLOCK_TOP_NULL_Check_PUSH,//5
	TOP_NULL_SETTING_N_RETURN_PUSH,//5
	CAS_BEFORE_PUSH,//6
	CAS_AFTER_PUSH,//7
	LOOP_OUT_PUSH,//8
	PUSH_LINE8,//9


	POP_ENTRY,//10
	TOP_COPY_POP,//11
	BEFRORE_CHANGE_SETTING_POP,//12
	AFTER_CHANGE_SETTING_POP,
	CAS_BEFORE_POP,//13
	CAS_AFTER_POP,//14
	LOOP_OUT_POP,//15
	DATA_SETTING_POP,//16
	DEL_NODE_POP//17


};
//MyMemoryLog<int64_t> g_MemoryLog;

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
	void Crash();
	int32_t GetStackCount();

	//--------------------------------
	// For Debugging
	//--------------------------------

	int32_t GetMemoryAllocCount();
public:
	TopCheck* m_TopCheck;
	LONG m_Count;

	FreeList<Node> m_MemoryPool;

};

template<typename T>
inline LockFreeStack<T>::LockFreeStack()
{
	m_Count=0;

	m_TopCheck = (TopCheck*)_aligned_malloc(sizeof(TopCheck), 16);
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
	
#if NEWDELTE ==1
	Node* newNode = new Node;
#endif 
#if NEWDELTE == 0
	Node* newNode = m_MemoryPool.Alloc();
#endif

	
	newNode->_Data = data;
	newNode->_Next = nullptr;

	TopCheck tempTop;
	
	tempTop._TopPtr = m_TopCheck->_TopPtr;
	tempTop._ID = m_TopCheck->_ID;
	do
	{
		newNode->_Next = tempTop._TopPtr;

	} while (!InterlockedCompareExchange128((LONG64*)m_TopCheck, (LONG64)tempTop._ID+1, (LONG64)newNode, (LONG64*)&tempTop));

	InterlockedIncrement(&m_Count);
	
	return true;
}

template<typename T>
inline bool LockFreeStack<T>::Pop(T* outData)
{
	if (InterlockedDecrement(&m_Count) < 0)
	{
		InterlockedIncrement(&m_Count);
		return false;
	}

	TopCheck tempTop;

	tempTop._TopPtr = m_TopCheck->_TopPtr;
	tempTop._ID = m_TopCheck->_ID;
	do
	{
	} while (!InterlockedCompareExchange128((LONG64*)m_TopCheck, (LONG64)tempTop._ID+1, (LONG64)tempTop._TopPtr->_Next, (LONG64*)&tempTop));
	*outData = tempTop._TopPtr->_Data;
#if NEWDELTE ==1
	delete tempTop._TopPtr;
	g_MemoryStackPtr.MemoryLogging(DEL_NODE_POP, GetCurrentThreadId(), 0, (int64_t)tempTop._TopPtr, (int64_t)m_TopCheck->_TopPtr);
#endif 
#if NEWDELTE == 0
	m_MemoryPool.Free(tempTop._TopPtr);
#endif

	return true;
}

template<typename T>
inline void LockFreeStack<T>::Crash()
{
	int* temp = nullptr;
	*temp = 10;
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
