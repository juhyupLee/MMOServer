#pragma once
#include <iostream>
#include <Windows.h>


template <typename T>
class TemplateQ
{
public:
	enum
	{
		BUFFER_SIZE = 500
	};

public:
	TemplateQ();
	~TemplateQ();
public:
	bool Enqueue(__in T data);
	bool Dequeue(__out T*  data);


	int32_t GetFreeSize() const; 
	int32_t GetUsedSize() const;

	void ClearBuffer();


private:
	int32_t GetFreeSize(int32_t front, int32_t rear) const;
	int32_t GetUsedSize(int32_t front, int32_t rear) const;

private:
	int32_t m_Front;
	int32_t m_Rear;
	T m_DataArray[BUFFER_SIZE];
};


template<typename T>
TemplateQ<T>::TemplateQ()
    :
    m_Front(0),
    m_Rear(0)
{
}

template<typename T>
TemplateQ<T>::~TemplateQ()
{
}


template<typename T>
int32_t TemplateQ<T>::GetFreeSize() const
{
    int32_t copyFront = m_Front;
    int32_t copyRear = m_Rear;

    int freeSize = 0;

    if ((copyRear + 1) % BUFFER_SIZE == copyFront)
    {
        return 0;
    }
    if (copyFront > copyRear)
    {
        freeSize = copyFront - copyRear - 1;
    }
    else
    {
        freeSize = copyFront + (BUFFER_SIZE - 1) - copyRear;
    }
    return freeSize;
}

template<typename T>
int32_t TemplateQ<T>::GetUsedSize() const
{
    int32_t copyRear = m_Rear;
    int32_t copyFront = m_Front;

    int32_t usedSize = 0;

    if (copyRear == copyFront)
    {
        return 0;
    }
    if (copyFront < copyRear)
    {
        usedSize = copyRear - copyFront;
    }
    else
    {
        usedSize = copyRear + (BUFFER_SIZE - 1) - copyFront + 1;
    }
    return usedSize;
}

template<typename T>
int32_t TemplateQ<T>::GetFreeSize(int32_t front, int32_t rear) const
{
    int freeSize = 0;
    if ((rear + 1) % BUFFER_SIZE == front)
    {
        return 0;
    }
    if (front > rear)
    {
        freeSize = front - rear - 1;
    }
    else
    {
        freeSize = front + (BUFFER_SIZE - 1) - rear;
    }

    return freeSize;
}

template<typename T>
int32_t TemplateQ<T>::GetUsedSize(int32_t front, int32_t rear) const
{
    int32_t usedSize = 0;

    if (rear == front)
    {
        return 0;
    }
    if (front < rear)
    {
        usedSize = rear - front;
    }
    else
    {
        usedSize = rear + (BUFFER_SIZE - 1) - front + 1;
    }

    return usedSize;
}

template<typename T>
void TemplateQ<T>::ClearBuffer()
{
    m_Front = 0;
    m_Rear = 0;
}


template<typename T>
bool TemplateQ<T>::Enqueue(T data)
{
    int32_t tempFront = m_Front;
    int32_t tempRear = m_Rear;

    if ((tempRear + 1) % BUFFER_SIZE == tempFront)
    {
        return false;
    }

    tempRear = (tempRear + 1) % BUFFER_SIZE;

    m_DataArray[tempRear] = data;


    m_Rear = tempRear;

    return true;
}

template<typename T>
bool TemplateQ<T>::Dequeue(T* data)
{
    int32_t tempRear = m_Rear;
    int32_t tempFront = m_Front;

    if (tempRear == tempFront)
    {
        return false;
    }

    tempFront = (tempFront + 1) % BUFFER_SIZE;

    *data = m_DataArray[tempFront];

    m_Front = tempFront;

    return true;
}
