#pragma once

class MemoryPool_TLS_Base
{
public:
	MemoryPool_TLS_Base() = default;
	virtual ~MemoryPool_TLS_Base() = default;

	virtual void* AllocMem(size_t size) = 0;
	virtual bool FreeMem(void* ptr) = 0;
};


struct ChunkMark
{
	// MarkID 가 필요없다 누구소속의 청크인지 ChunkPtr로 구분해주면된다  
	void* _ChunkPtr;
	int64_t _MarkValue;
	size_t _Size;
	size_t _ThreadID;
};

template <typename T>
class MemoryPool_TLS : public MemoryPool_TLS_Base
{
	struct ChunkMemoryHeader
	{
	public:
		ChunkMemoryHeader() = default;
		~ChunkMemoryHeader() = default;
		ChunkMark _FrontMark;
		T _Data;
		ChunkMark _RearMark;
	};
	class ChunkMemory
	{
	public:

	public:
		ChunkMemory() = default;
		~ChunkMemory() = default;
	public:
		T* Alloc(size_t size = 0);
		void Free(T* data);


	public:
		void AllocInit(DWORD objectCount, MemoryPool_TLS<T>* centerPool);
	public:
		std::stack<ChunkMemoryHeader> m_Chunk;
		LockFreeStack<ChunkMemoryHeader> m_DelaydFree;
		MemoryPool_TLS<T>* m_CenterMemoryPool;
		DWORD m_ObjectCount;
		DWORD m_AllocIndex;
		LONG  m_FreeCount;
	};
public:
	MemoryPool_TLS();
	MemoryPool_TLS(DWORD objectCount);
	~MemoryPool_TLS();
public:

	virtual void* AllocMem(size_t size) override;
	virtual bool FreeMem(void* ptr) override;
	void FreeLoal(ChunkMemory* data);
	T* Alloc(size_t size = 0);
	bool Free(T* data);
	void Crash();

	ChunkMemory* LocalChunkAlloc()
	{
		auto tlsStack = (std::stack<ChunkMemory*>*)TlsGetValue(m_LocalMemoryPoolIndex);
		if (tlsStack == nullptr)
		{
			return nullptr;
		}
		if(tlsStack->empty())
		{
			return nullptr;
		}
		auto rtnData = tlsStack->top();
		tlsStack->pop();
		return rtnData;
	}

	ChunkMemory* ChunkAlloc()
	{
		auto chunkPtr = m_ChunkMemoryPool.AllocateMemoryFromHeap();
		chunkPtr->AllocInit(m_ObjectCount, this);
		TlsSetValue(m_TLSChunkIndex, chunkPtr);

		return chunkPtr;
	}
	int32_t GetChunkCount();
	int32_t GetPoolCount();
	int32_t GetUseCount();

public:

	FreeList<ChunkMemory> m_ChunkMemoryPool;
	DWORD m_TLSChunkIndex;
	DWORD m_LocalMemoryPoolIndex;
	DWORD m_ObjectCount;
};


template<typename T>
inline T* MemoryPool_TLS<T>::ChunkMemory::Alloc(size_t size)
{
	//-------------------------------------
	// Chunk의 Mark부분을 제외한 진짜  T타입의 포인터를 던져준다.
	//------------------------------------
	//-------------------------------------------------------------------------------
	// Alloc은 각 스레드에서 진행 되기 때문에, 굳이 Interlock으로 ++ 할필요없다
	//-------------------------------------------------------------------------------

	auto chunkHeader = m_Chunk.top();
	T* rtnData = (T*)((char*)&chunkHeader + sizeof(ChunkMark));
	chunkHeader._FrontMark._Size = size;
	chunkHeader._FrontMark._ThreadID = GetCurrentThreadId();
	m_Chunk.pop();

	//T* rtnData = (T*)((char*)&m_Chunk[--m_AllocIndex] + sizeof(ChunkMark));
	//m_Chunk[m_AllocIndex]._FrontMark._Size = size;

	//std::construct_at(rtnData);

	//---------------------------------------
	// Return 하는순간 다른스레드에서 바로 반납할수있기때문에
	// Return 하기전에 TLS의 포인터를 새로 세팅해준다.
	//---------------------------------------
	//if (m_AllocIndex == 0)
	//{
	//	m_CenterMemoryPool->ChunkAlloc();
	//}

	if(m_Chunk.empty())
	{
		ChunkMemoryHeader data;
		if(m_DelaydFree.Pop(&data) == true)
		{
			m_Chunk.push(data);
		}
		else
		{
			AllocInit(m_ObjectCount, m_CenterMemoryPool);
		}
	}

	return rtnData;
}

template<typename T>
inline void MemoryPool_TLS<T>::ChunkMemory::Free(T* data)
{
	ChunkMemoryHeader* dataPtr = (ChunkMemoryHeader*)((char*)data - sizeof(ChunkMark));

	//----------------------------------------------------
	// 반납된 포인터가 언더플로우 한 경우
	//----------------------------------------------------
	if (dataPtr->_FrontMark._MarkValue != MARK_FRONT)
	{
		throw(FreeListException(L"Underflow Violation", __LINE__));
		return;
	}
	//----------------------------------------------------
	// 오버플로우 체크
	//----------------------------------------------------
	if (dataPtr->_RearMark._ChunkPtr != this || dataPtr->_RearMark._MarkValue != MARK_REAR)
	{
		throw(FreeListException(L"Overflow Violation", __LINE__));
		return;
	}

	//--------------------------------------------
	// 사용자가 placement 를 사용한다면
	// Free할때 소멸자를 호출해준다
	// 안쓴다면 굳이 소멸자를 호출해 줄 필요는 없다.
	//--------------------------------------------
	//if (m_bPlacementNew)
	//{
	//	data->~T();
	//}

	//-------------------------------------
	// Alloc같은경우는 TLS에서 데이터를 Alloc하기때문에 여러스레드에서 접근할 확률이없지만
	// Free는 떠다니는 포인터에 어떤스레드든 접근가능하기때문에 여러스레드에서 접근할 수 있다.
	//-------------------------------------
	
		if(dataPtr->_FrontMark._ThreadID == GetCurrentThreadId())
		{
			//m_CenterMemoryPool->FreeLoal(this);
			m_Chunk.push(*dataPtr);

		}
		else
		{
		/*	if (InterlockedDecrement(&m_FreeCount) == m_ObjectCount)
			{

			}*/

			m_DelaydFree.Push(*dataPtr);
			//m_CenterMemoryPool->m_ChunkMemoryPool.Free(this);
		}

		return;
	
}

template <typename T>
void MemoryPool_TLS<T>::FreeLoal(ChunkMemory* data)
{
	auto tlsStack = (std::stack<ChunkMemory*>*)TlsGetValue(m_LocalMemoryPoolIndex);
	if(tlsStack == nullptr)
	{
		tlsStack = new std::stack<ChunkMemory*>();
		TlsSetValue(m_LocalMemoryPoolIndex, tlsStack);
	}
	tlsStack->push(data);
}


template <typename T>
void* MemoryPool_TLS<T>::AllocMem(size_t size)
{
	return reinterpret_cast<void*>(Alloc(size));
}

template <typename T>
bool MemoryPool_TLS<T>::FreeMem(void* ptr)
{
	return Free(reinterpret_cast<T*>(ptr));
}

template<typename T>
void MemoryPool_TLS<T>::ChunkMemory::AllocInit(DWORD objectCount, MemoryPool_TLS<T>* centerPool)
{
	if (m_Chunk.empty())
	{
		m_CenterMemoryPool = centerPool;
		m_ObjectCount = objectCount;

		m_AllocIndex = m_ObjectCount;
		m_FreeCount = m_ObjectCount;
		
		for (size_t i = 0; i < m_ObjectCount; ++i)
		{
			ChunkMemoryHeader chunkHeader;
			chunkHeader._FrontMark._ThreadID = GetCurrentThreadId();
			chunkHeader._FrontMark._ChunkPtr = this;
			chunkHeader._FrontMark._MarkValue = MARK_FRONT;
			chunkHeader._RearMark._ChunkPtr = this;
			chunkHeader._RearMark._MarkValue = MARK_REAR;
			m_Chunk.push(chunkHeader);
		}
	}
	else
	{
		m_FreeCount = m_ObjectCount;
		m_AllocIndex = m_ObjectCount;
	}
}

template<typename T>
inline MemoryPool_TLS<T>::MemoryPool_TLS()
{
	m_ObjectCount = 50000;
	m_TLSChunkIndex = TlsAlloc();
	if (m_TLSChunkIndex == TLS_OUT_OF_INDEXES)
	{
		Crash();
	}

	m_LocalMemoryPoolIndex = TlsAlloc();
	if (m_LocalMemoryPoolIndex == TLS_OUT_OF_INDEXES)
	{
		Crash();
	}
	//ChunkSetting();
}

template<typename T>
inline MemoryPool_TLS<T>::MemoryPool_TLS(DWORD objectCount)
{
	m_ObjectCount = objectCount;
	m_TLSChunkIndex = TlsAlloc();
	if (m_TLSChunkIndex == TLS_OUT_OF_INDEXES)
	{
		Crash();
	}
}

template<typename T>
inline MemoryPool_TLS<T>::~MemoryPool_TLS()
{
	TlsFree(m_TLSChunkIndex);
}

template<typename T>
inline T* MemoryPool_TLS<T>::Alloc(size_t size)
{
	ChunkMemory* chunkPtr = LocalChunkAlloc();
	if(chunkPtr == nullptr)
	{
		chunkPtr = (ChunkMemory*)TlsGetValue(m_TLSChunkIndex);
		if (chunkPtr == nullptr)
		{
			chunkPtr = ChunkAlloc();
		}
	}
	else
	{
		chunkPtr->AllocInit(m_ObjectCount, this);
	}
	return chunkPtr->Alloc(size);
}

template<typename T>
inline bool MemoryPool_TLS<T>::Free(T* data)
{
	MemoryPool_TLS<T>::ChunkMemoryHeader* dataPtr= (MemoryPool_TLS<T>::ChunkMemoryHeader*)((char*)data - sizeof(ChunkMark));
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
