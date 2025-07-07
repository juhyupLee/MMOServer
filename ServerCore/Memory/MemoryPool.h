#pragma once
//#include "../../mimalloc.h"
#include "../Test/Test_LockFreeQ.h"

class FreeListBase;
class MemoryPool_TLS_Base;
class MemoryPool
{
	enum
	{
		PAGE_SIZE = 4096
	};
public:
	MemoryPool();

	void* Alloc(size_t size);
	void Free(void* ptr);

private:
//	std::vector<FreeListBase*> m_memory;
	std::vector<MemoryPool_TLS_Base*> m_memory;
};

extern MemoryPool gMemoryPool;
template <class T>
class MyAllocator
{
public:
	MyAllocator() = default;

	using value_type = T;
	using pointer = value_type*;
	using const_pointer = const value_type*;
	using reference = value_type&;
	using const_reference = const value_type&;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	template <class U>
	MyAllocator(const MyAllocator<U>&)
	{
	}

	template <class U>
	struct rebind
	{
		using other = MyAllocator<U>;
	};

	void construct(pointer p, T&& t)
	{
		new(p)T(std::forward<T>(t));
	}

	void destroy(pointer p)
	{
		p->~T();
	}

	T* allocate(size_t n)
	{
		return static_cast<T*>(gMemoryPool.Alloc((int)n * sizeof(T)));
		//return static_cast<T*>(mi_malloc((int)n * sizeof(T)));
	}

	void deallocate(T* ptr, size_t n)
	{
		n;
		gMemoryPool.Free(ptr);
		//mi_free(ptr);
	}

	template<class U>
	bool operator!=(const MyAllocator<U>& _Right)
	{	// test for inequality
		return (!(*this == _Right));
	}
};
template<size_t SIZE>
struct Buffer
{
	char data[SIZE];
};

