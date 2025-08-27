#pragma once
template <typename T, int32_t DEFAULT_SIZE = 5000> requires std::is_pointer_v<T>
class PointerStack
{
public:
	PointerStack() {};
	~PointerStack() {};

public:
	bool Push(T data)
	{
		if(Full())
		{
			return false;
		}

		m_stack[m_top++] = data;

		return true;
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


	bool Full()
	{
		return m_top > DEFAULT_SIZE - 1;
	}


	int32_t Size()
	{
		return m_top;
	}
private:
	int32_t m_top{0};
	T m_stack[DEFAULT_SIZE];
};