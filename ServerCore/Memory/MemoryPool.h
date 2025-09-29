#pragma once

class FreeListBase;
class ObjectPoolBase;
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
	ObjectPoolBase* m_memory[PAGE_SIZE + 1];
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

	template<typename U, typename... Args>
	void construct(U* p, Args&&... args)
	{
		::new((void*)p) U(std::forward<Args>(args)...);
	}

	template<typename U>
	void destroy(U* p)
	{
		p->~U();
	}

	T* allocate(size_t n)
	{
		return static_cast<T*>(gMemoryPool.Alloc(static_cast<int>(n) * sizeof(T)));
	}

	void deallocate(T* ptr, size_t n)
	{
		n;
		gMemoryPool.Free(ptr);
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

template<typename T, typename... Args>
std::shared_ptr<T> MakeMySharedPtr(Args&&... args)
{
	return std::allocate_shared<T>(MyAllocator<T>(), std::forward<Args>(args)...);
}