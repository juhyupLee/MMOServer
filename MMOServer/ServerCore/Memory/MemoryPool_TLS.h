#pragma once
#include "FreeList.h"

template <typename T>
class MemoryPool_TLS
{
	struct ChunkMark
	{
		// MarkID �� �ʿ���� �����Ҽ��� ûũ���� ChunkPtr�� �������ָ�ȴ�  
		void* _ChunkPtr;
		int64_t _MarkValue;
	};

	struct ChunkAllocMemory
	{
		ChunkMark _FrontMark;
		T _Data;
		ChunkMark _RearMark;
	};
	class ChunkMemory
	{
	public:
		
	public:
		ChunkMemory();
		~ChunkMemory();
	public:
		T* Alloc();
		bool Free(T* data);

	public:
		void AllocInit(bool placementNew, DWORD objectCount, MemoryPool_TLS<T>* centerPool);
	public:
		ChunkAllocMemory* m_Chunk;
		MemoryPool_TLS<T>* m_CenterMemoryPool;
		bool m_bPlacementNew;
		DWORD m_ObjectCount;
		DWORD m_AllocIndex;
		LONG  m_FreeCount;
	};


public:
	MemoryPool_TLS(DWORD objectCount,bool bPlacementNew = false);
	~MemoryPool_TLS();
public:
	T* Alloc();
	bool Free(T* data);
	void Crash();
	ChunkMemory* ChunkSetting()
	{
		MemoryPool_TLS<T>::ChunkMemory* chunkPtr = m_ChunkMemoryPool.Alloc();
		chunkPtr->AllocInit(m_bPlacementNew, m_ObjectCount, this);
		TlsSetValue(m_TLSChunkIndex, chunkPtr);
		
		return chunkPtr;
	}
	int32_t GetChunkCount();
	int32_t GetPoolCount();
	int32_t GetUseCount();

private:

	FreeList <ChunkMemory> m_ChunkMemoryPool;
	DWORD m_TLSChunkIndex;
	bool m_bPlacementNew;
	DWORD m_ObjectCount;
};


template<typename T>
inline MemoryPool_TLS<T>::ChunkMemory::ChunkMemory()
{
	m_Chunk = nullptr;
	m_CenterMemoryPool = nullptr;

}

template<typename T>
inline MemoryPool_TLS<T>::ChunkMemory::~ChunkMemory()
{
	delete[] m_Chunk;
}



template<typename T>
inline T* MemoryPool_TLS<T>::ChunkMemory::Alloc()
{
	//-------------------------------------
	// Chunk�� Mark�κ��� ������ ��¥  TŸ���� �����͸� �����ش�.
	//------------------------------------

	//-------------------------------------------------------------------------------
	// Alloc�� �� �����忡�� ���� �Ǳ� ������, ���� Interlock���� ++ ���ʿ����
	//-------------------------------------------------------------------------------
	T* rtnData = (T*)((char*)&m_Chunk[--m_AllocIndex] + sizeof(ChunkMark));


	if (m_bPlacementNew )
	{
		new(rtnData) T;
	}

	//---------------------------------------
	// Return �ϴ¼��� �ٸ������忡�� �ٷ� �ݳ��Ҽ��ֱ⶧����
	// Return �ϱ����� TLS�� �����͸� ���� �������ش�.
	//---------------------------------------
	if (m_AllocIndex == 0)
	{
		m_CenterMemoryPool->ChunkSetting();
	}

	return rtnData;
}

template<typename T>
inline bool MemoryPool_TLS<T>::ChunkMemory::Free(T* data)
{

	ChunkAllocMemory* dataPtr = (ChunkAllocMemory*)((char*)data - sizeof(ChunkMark));


	//----------------------------------------------------
	// �ݳ��� �����Ͱ� ����÷ο� �� ���
	//----------------------------------------------------
	if (dataPtr->_FrontMark._MarkValue != MARK_FRONT)
	{
		throw(FreeListException(L"Underflow Violation", __LINE__));
		return false;
	}
	//----------------------------------------------------
	// �����÷ο� üũ
	//----------------------------------------------------
	if (dataPtr->_RearMark._ChunkPtr != this || dataPtr->_RearMark._MarkValue != MARK_REAR)
	{
		throw(FreeListException(L"Overflow Violation", __LINE__));
		return false;
	}

	//--------------------------------------------
	// ����ڰ� placement �� ����Ѵٸ�
	// Free�Ҷ� �Ҹ��ڸ� ȣ�����ش�
	// �Ⱦ��ٸ� ���� �Ҹ��ڸ� ȣ���� �� �ʿ�� ����.
	//--------------------------------------------
	if (m_bPlacementNew)
	{
		data->~T();
	}

	//-------------------------------------
	// Alloc�������� TLS���� �����͸� Alloc�ϱ⶧���� ���������忡�� ������ Ȯ���̾�����
	// Free�� ���ٴϴ� �����Ϳ� �������� ���ٰ����ϱ⶧���� ���������忡�� ������ �� �ִ�.
	//-------------------------------------
	if (InterlockedDecrement(&m_FreeCount) == 0)
	{
		m_CenterMemoryPool->m_ChunkMemoryPool.Free(this);
		return true;
	}
	return false;
}

template<typename T>
__forceinline void MemoryPool_TLS<T>::ChunkMemory::AllocInit(bool placementNew, DWORD objectCount, MemoryPool_TLS<T>* centerPool)
{
	if (m_Chunk == nullptr)
	{
		m_CenterMemoryPool = centerPool;
		m_ObjectCount = objectCount;
		m_bPlacementNew = placementNew;

		m_AllocIndex = m_ObjectCount;
		m_FreeCount = m_ObjectCount;

		if (m_bPlacementNew)
		{
			m_Chunk = (ChunkAllocMemory*)malloc(sizeof(ChunkAllocMemory) * m_ObjectCount);
		}
		else
		{
			m_Chunk = new ChunkAllocMemory[m_ObjectCount];
		}
		

		for (size_t i = 0; i < m_ObjectCount; ++i)
		{
			m_Chunk[i]._FrontMark._ChunkPtr = this;
			m_Chunk[i]._FrontMark._MarkValue = MARK_FRONT;

			m_Chunk[i]._RearMark._ChunkPtr = this;
			m_Chunk[i]._RearMark._MarkValue = MARK_REAR;
		}
	}
	else
	{
		m_FreeCount = m_ObjectCount;
		m_AllocIndex = m_ObjectCount;
	}
}

template<typename T>
inline MemoryPool_TLS<T>::MemoryPool_TLS(DWORD objectCount,bool bPlacementNew)
{
	m_ObjectCount = objectCount;
	m_TLSChunkIndex = TlsAlloc();
	if (m_TLSChunkIndex == TLS_OUT_OF_INDEXES)
	{
		Crash();
	}

	m_bPlacementNew = bPlacementNew;
}

template<typename T>
inline MemoryPool_TLS<T>::~MemoryPool_TLS()
{
	TlsFree(m_TLSChunkIndex);
}

template<typename T>
inline T* MemoryPool_TLS<T>::Alloc()
{
	ChunkMemory* chunkPtr= (ChunkMemory *)TlsGetValue(m_TLSChunkIndex);

	if (chunkPtr == nullptr)
	{
	
		chunkPtr = ChunkSetting();
	}

	return chunkPtr->Alloc();
}

template<typename T>
inline bool MemoryPool_TLS<T>::Free(T* data)
{

	MemoryPool_TLS<T>::ChunkAllocMemory* dataPtr= (MemoryPool_TLS<T>::ChunkAllocMemory*)((char*)data - sizeof(ChunkMark));
	((ChunkMemory*)(dataPtr->_FrontMark._ChunkPtr))->Free(data);
	
	return true;
}

template<typename T>
inline void MemoryPool_TLS<T>::Crash()
{
	int* p = nullptr;
	*p = 10;
}


//template<typename T>
//inline MemoryPool_TLS<T>::ChunkMemory* MemoryPool_TLS<T>::ChunkSetting()
//{
//	MemoryPool_TLS<T>::ChunkMemory* chunkPtr = m_ChunkMemoryPool.
// ();
//	chunkPtr->AllocInit(m_bPlacementNew, m_ObjectCount,this);
//	TlsSetValue(m_TLSChunkIndex, chunkPtr);
//
//	return chunkPtr;
//
//}

template<typename T>
inline int32_t MemoryPool_TLS<T>::GetChunkCount()
{
	return  m_ChunkMemoryPool.GetAllocCount();
}

template<typename T>
inline int32_t MemoryPool_TLS<T>::GetPoolCount()
{
	return m_ChunkMemoryPool.GetPoolCount();
}

template<typename T>
inline int32_t MemoryPool_TLS<T>::GetUseCount()
{
	return m_ChunkMemoryPool.GetUseCount();
}
