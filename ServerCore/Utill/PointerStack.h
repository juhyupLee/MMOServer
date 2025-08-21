#pragma once
template <typename T> requires std::is_pointer_v<T>
class PointerStack
{
public:
	PointerStack(int32_t size)
		:m_size(size)
	{
		m_stack = new T[size];
	}

	~PointerStack()
	{
		delete[] m_stack;
	}
public:
	void Push(T data)
	{
		if(m_top > m_size - 1)
		{
			return;
		}

		m_stack[m_top++] = data;
	}

	T Pop()
	{
		if(m_top - 1 < 0)
		{
			return nullptr;
		}
		return m_stack[--m_top];
	}

	bool Empty()
	{
		return m_top == 0;
	}
private:
	int32_t m_top{0};
	int32_t m_size;
	T* m_stack;
};