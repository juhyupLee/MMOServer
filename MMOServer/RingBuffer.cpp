#include "RingBuffer.h"
#include <iostream>
#include <Windows.h>
#include "Log.h"
#include "MemoryLog.h"


enum
{
    BEFRE_REAR,
    BEFORE_FRONT,
    AFTER_REAR,
    AFTER_FRONT
};

RingQ::RingQ()
    :
    m_Buffer{ 0, },
    m_Front(0),
    m_Rear(0)
{
    int a = 10;
}

RingQ::~RingQ()
{
}

int RingQ::Enqueue(char* buffer, int32_t size, Session* curSession)
{
    int32_t copyFront = m_Front;
    int32_t copyRear = m_Rear;

    int32_t copyFreeSize = GetFreeSize(copyFront,copyRear);

    //-----------------------------------
    // 링버퍼의 남은 사이즈가 들어온 사이즈보다 작다면 size를 프리사이즈로바꿈
    //-----------------------------------
    if (copyFreeSize < size)
    {
        size = copyFreeSize;
        if (size <= 0)
        {
            return 0;
        }
    }
    int32_t diff = 0;
    int32_t directSize = GetDirectEnqueueSize(copyFront, copyRear);

    //-----------------------------------
    // Enqueue 들어오면 m_Rear는 쁠쁠 먼저하고, Enqeue를한다.
    //-----------------------------------
    copyRear = (copyRear + 1) % RING_BUFFER_SIZE;

    //-----------------------------------
    // 인자로 들어온 사이즈가  연속된 사이즈 보다 더 크다면, memcpy를 나눠서해야한다
    //-----------------------------------
    if (directSize < size)
    {
        memcpy(m_Buffer + copyRear, buffer, directSize);
        copyRear = (copyRear +directSize-1) % RING_BUFFER_SIZE;
        diff = size - directSize;

        copyRear = (copyRear + 1) % RING_BUFFER_SIZE;
        memcpy(m_Buffer + copyRear, buffer + directSize, diff);
        copyRear = (copyRear + diff-1) % RING_BUFFER_SIZE;
    }
    else
    {
        memcpy(m_Buffer + copyRear, buffer, size);
        copyRear = (copyRear +size-1) % RING_BUFFER_SIZE;
    }

    m_Rear = copyRear;

    return size;
}

int RingQ::Enqueue(char* buffer, int32_t size)
{

    int32_t copyFront = m_Front;
    int32_t copyRear = m_Rear;

    int32_t copyFreeSize = GetFreeSize(copyFront, copyRear);

    //-----------------------------------
    // 링버퍼의 남은 사이즈가 들어온 사이즈보다 작다면 size를 프리사이즈로바꿈
    //-----------------------------------
    if (copyFreeSize < size)
    {
        size = copyFreeSize;
        if (size <= 0)
        {
            return 0;
        }
    }
    int32_t diff = 0;
    int32_t directSize = GetDirectEnqueueSize(copyFront, copyRear);

    //-----------------------------------
    // Enqueue 들어오면 m_Rear는 쁠쁠 먼저하고, Enqeue를한다.
    //-----------------------------------
    copyRear = (copyRear + 1) % RING_BUFFER_SIZE;

    //-----------------------------------
    // 인자로 들어온 사이즈가  연속된 사이즈 보다 더 크다면, memcpy를 나눠서해야한다
    //-----------------------------------
    if (directSize < size)
    {
        memcpy(m_Buffer + copyRear, buffer, directSize);
        copyRear = (copyRear + directSize - 1) % RING_BUFFER_SIZE;
        diff = size - directSize;

        copyRear = (copyRear + 1) % RING_BUFFER_SIZE;
        memcpy(m_Buffer + copyRear, buffer + directSize, diff);
        copyRear = (copyRear + diff - 1) % RING_BUFFER_SIZE;
    }
    else
    {
        memcpy(m_Buffer + copyRear, buffer, size);
        copyRear = (copyRear + size - 1) % RING_BUFFER_SIZE;
    }

    m_Rear = copyRear;

    return size;
}

int RingQ::Dequeue(char* buffer, int32_t size)
{
    int copyRear = m_Rear;
    int copyFront = m_Front;
    int copyUsedSize = GetUsedSize(copyFront, copyRear);

    //-------------------------------------------
    //링버퍼에 사용중인 사이즈가 인자로 들어온 사이즈보다 작으면 실패
    //-------------------------------------------
    if (copyUsedSize < size)
    {
        size = copyUsedSize;
        if (size <= 0)
        {
            return 0;
        }
    }

    int directSize = GetDirectDequeueSize(copyFront,copyRear);

    //-----------------------------------
    // Enqueue 들어오면 m_Rear는 쁠쁠 먼저하고, Enqeue를한다.
    //-----------------------------------
    copyFront = (copyFront + 1) % RING_BUFFER_SIZE;
    //-------------------------------------------
    // 연속할당할수있는 사이즈가 들어온 사이즈보다 작다면 두번나눠서 디큐를 해야함.
    //-------------------------------------------
    if (directSize < size)
    {
        memcpy(buffer, m_Buffer + copyFront, directSize);
        copyFront = (copyFront + directSize -1) % RING_BUFFER_SIZE;;

        int diff = size - directSize;
        copyFront = (copyFront + 1) % RING_BUFFER_SIZE;
        memcpy(buffer + directSize, m_Buffer + copyFront, diff);
        copyFront = (copyFront +diff -1 ) % RING_BUFFER_SIZE;
    }
    else
    {
        memcpy(buffer, m_Buffer + copyFront, size);
        copyFront = (copyFront +size -1 ) % RING_BUFFER_SIZE;
    }

    m_Front = copyFront;
 
    return size;
}

int32_t RingQ::GetFreeSize() const
{
    int32_t copyFront = m_Front;
    int32_t copyRear = m_Rear;

    int freeSize = 0;

    if ((copyRear + 1) % RING_BUFFER_SIZE == copyFront)
    {
        return 0;
    }
    if (copyFront > copyRear)
    {
        freeSize = copyFront - copyRear -1;
    }
    else
    {
        freeSize = copyFront + (RING_BUFFER_SIZE - 1) - copyRear;
    }
    return freeSize;
}

int32_t RingQ::GetUsedSize() const
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
        usedSize = copyRear + (RING_BUFFER_SIZE - 1) - copyFront + 1;
    }
    return usedSize;
}

int32_t RingQ::GetFreeSize(int32_t front, int32_t rear) const
{
    int freeSize = 0;
    if ((rear + 1) % RING_BUFFER_SIZE == front)
    {
        return 0;
    }
    if (front > rear)
    {
        freeSize = front - rear-1;
    }
    else
    {
        freeSize = front + (RING_BUFFER_SIZE - 1) - rear;
    }

    return freeSize;
}

int32_t RingQ::GetUsedSize(int32_t front, int32_t rear) const
{
    int32_t usedSize = 0;

    if (rear == front)
    {
        return 0;
    }
    if (front < rear)
    {
        usedSize = rear - front ;
    }
    else
    {
        usedSize = rear+ (RING_BUFFER_SIZE - 1) - front + 1;
    }

    return usedSize;
}

int32_t RingQ::GetDirectEnqueueSize() const
{
     //-----------------------------------
    // 링버퍼의 사이즈가 꽉찼다면, Direct Size를 구할수 없다. 0반환
    //-----------------------------------
    int copyFront = m_Front;
    int copyRear = m_Rear;

    if ((copyRear + 1) % RING_BUFFER_SIZE == copyFront)
    {
        return 0;
    }

    int directSize = 0;
    int rearNext = (copyRear + 1) % RING_BUFFER_SIZE;

    //-----------------------------------
    // mRear < mFront < 인덱스 끝
    // 이러면  연속된 인덱스는 m_Front 전까지다.
    //-----------------------------------

    if (rearNext < copyFront)
    {
        directSize = copyFront - rearNext;
    }
    //-----------------------------------
    // 그것이 아니라면, m_Rear ~ 인덱스끝까지가 연속된 메모리 할당범위다.
    //-----------------------------------
    else
    {
        directSize = (RING_BUFFER_SIZE - 1) - rearNext + 1;
    }

    return directSize;
}

void RingQ::GetDirectEnQData(DirectData* directSize) 
{
    int32_t  copyFront = m_Front;
    int32_t copyRear = m_Rear;


    (*directSize).bufferPtr1 = GetRearBufferPtr(copyFront, copyRear);

    int directEnQSize = GetDirectEnqueueSize(copyFront,copyRear);

    (*directSize)._Direct1 = directEnQSize;


    copyRear = (copyRear +directEnQSize) % RING_BUFFER_SIZE;

    (*directSize).bufferPtr2 = GetRearBufferPtr(copyFront, copyRear);
    (*directSize)._Direct2 = GetDirectEnqueueSize(copyFront, copyRear);
 

  
}

void RingQ::GetDirectDeQData(DirectData* directSize, Session* curSession)
{
    
    int32_t copyRear = m_Rear;
    int32_t  copyFront = m_Front;

    (*directSize).bufferPtr1 = GetFrontBufferPtr(copyFront, copyRear);

    int directDeQSize = GetDirectDequeueSize(copyFront, copyRear);

  
    (*directSize)._Direct1 = directDeQSize;


    copyFront = (copyFront + directDeQSize ) % RING_BUFFER_SIZE;

    (*directSize).bufferPtr2 = GetFrontBufferPtr(copyFront, copyRear);
    (*directSize)._Direct2 = GetDirectDequeueSize(copyFront, copyRear);

}

void RingQ::GetDirectDeQData(DirectData* directSize)
{
    int32_t copyRear = m_Rear;
    int32_t  copyFront = m_Front;

    (*directSize).bufferPtr1 = GetFrontBufferPtr(copyFront, copyRear);

    int directDeQSize = GetDirectDequeueSize(copyFront, copyRear);


    (*directSize)._Direct1 = directDeQSize;


    copyFront = (copyFront + directDeQSize) % RING_BUFFER_SIZE;

    (*directSize).bufferPtr2 = GetFrontBufferPtr(copyFront, copyRear);
    (*directSize)._Direct2 = GetDirectDequeueSize(copyFront, copyRear);
}

int32_t RingQ::GetDirectDequeueSize() const
{
    int32_t copyRear = m_Rear;
    int32_t copyFront = m_Front;
    
    //------------------------------------------
    // 링버퍼가 비어있다면, 0을반환
    //------------------------------------------
    if (copyFront == copyRear)
    {
        return 0;
    }

    int32_t nextFront = (copyFront + 1) % RING_BUFFER_SIZE;

    int32_t directSize = 0;
    //일반적인 경우
    if (nextFront <= copyRear)
    {
        directSize = copyRear - nextFront+1;
    }
    else
    {
        directSize = (RING_BUFFER_SIZE - 1) - nextFront+1;
    }

    return directSize;
}

void RingQ::ClearBuffer()
{
    m_Front = 0;
    m_Rear = 0;
}

void RingQ::MoveRear(int size)
{
    if (size < 0)
    {
        return;
    }

    int copyRear = m_Rear;
    m_Rear = (copyRear + size) % RING_BUFFER_SIZE;
}

void RingQ::MoveFront(int size)
{
    if (size < 0)
    {
        return;
    }
    int copyFront = m_Front;
    m_Front = (copyFront + size ) % RING_BUFFER_SIZE;

}

void RingQ::MoveFrontTest(int size, Session* curSession)
{
    if (size < 0)
    {
        return;
    }
    int copyFront = m_Front;
    m_Front = (copyFront + size) % RING_BUFFER_SIZE;
}

char* RingQ::GetFrontBufferPtr(int32_t copyFront, int32_t copyRear)
{
    if (copyRear == copyFront)
    {
        return nullptr;
    }
    return m_Buffer + ((copyFront + 1) % RING_BUFFER_SIZE);
}

char* RingQ::GetFrontBufferPtr(void)
{
    //--------------------------------------
    // FrontBufferPtr을 쓰려고하는것은 지금
    // dequeue를하려는 상황인데, 큐가 비어있다면
    // null을반환
    //--------------------------------------
    
    int32_t copyRear = m_Rear;
    int32_t copyFront = m_Front;

    if (copyRear == copyFront)
    {
        return nullptr;
    }
    return m_Buffer + ((copyFront+1)% RING_BUFFER_SIZE); 
}

char* RingQ::GetRearBufferPtr(int32_t copyFront, int32_t copyRear)
{
    if ((copyRear + 1) % RING_BUFFER_SIZE == copyFront)
    {
        return nullptr;
    }
    return m_Buffer + ((copyRear + 1) % RING_BUFFER_SIZE);
}

char* RingQ::GetRearBufferPtr(void)
{
    //--------------------------------------
    // Enqueue를하려는 상황인데, 큐가 꽉차있다면,
    // null을반환
    //--------------------------------------
    int32_t copyFront = m_Front;
    int32_t copyRear = m_Rear;
    
    if ((copyRear +1)%RING_BUFFER_SIZE == copyFront)
    {
        return nullptr;
    }

    return m_Buffer + ((copyRear + 1) % RING_BUFFER_SIZE);
}

int RingQ::Peek(char* dest, int size)
{
    //-------------------------------------------
   //링버퍼에 사용중인 사이즈가 인자로 들어온 사이즈보다 작으면 현재 사용중인 사이즈로바꿈.
   // Peek이기때문에 프론트 사본 만듬.
   //-------------------------------------------
    int32_t copyRear = m_Rear;
    int32_t copyFront = m_Front;
    int tempUsedSize = GetUsedSize(copyFront,copyRear);

    if (tempUsedSize < size)
    {
        size = tempUsedSize;
        if (size <= 0)
        {
            return 0;
        }
    }

    int directSize = GetDirectDequeueSize();


    //-----------------------------------
    // Peek 들어오면 사본 front 는 쁠쁠 먼저하고, dequeue를한다.
    //-----------------------------------
    copyFront = (copyFront + 1) % RING_BUFFER_SIZE;

    //-------------------------------------------
    // 연속할당할수있는 사이즈가 들어온 사이즈보다 작다면 두번나눠서 디큐를 해야함.
    //-------------------------------------------
    if (directSize < size)
    {
        memcpy(dest, m_Buffer + copyFront, directSize);
        copyFront = (copyFront + directSize - 1) % RING_BUFFER_SIZE;;

        int diff = size - directSize;
        copyFront = (copyFront + 1) % RING_BUFFER_SIZE;
        memcpy(dest + directSize, m_Buffer + copyFront, diff);
    }
    else
    {
        memcpy(dest, m_Buffer + copyFront, size);
    }

    return size;
}

int32_t RingQ::GetFront()
{
    return m_Front;
}

int32_t RingQ::GetRear()
{
    return m_Rear;
}

int32_t RingQ::GetDirectEnqueueSize(int32_t copyFront, int32_t copyRear) const
{
    //-----------------------------------
    // 링버퍼의 사이즈가 꽉찼다면, Direct Size를 구할수 없다. 0반환
    //-----------------------------------

    if ((copyRear + 1) % RING_BUFFER_SIZE == copyFront)
    {
        return 0;
    }

    int directSize = 0;
    int rearNext = (copyRear + 1) % RING_BUFFER_SIZE;


    if (rearNext < copyFront)
    {
        directSize = copyFront - rearNext;
    }
    //-----------------------------------
    // 그것이 아니라면, m_Rear ~ 인덱스끝까지가 연속된 메모리 할당범위다.
    //-----------------------------------
    else
    {
        directSize = (RING_BUFFER_SIZE - 1) - rearNext + 1;
    }

    return directSize;
}

int32_t RingQ::GetDirectDequeueSize(int32_t copyFront, int32_t copyRear) const
{

    //------------------------------------------
    // 링버퍼가 비어있다면, 0을반환
    //------------------------------------------
    if (copyFront == copyRear)
    {
        return 0;
    }

    int nextFront = (copyFront + 1) % RING_BUFFER_SIZE;

    int32_t directSize = 0;
    //일반적인 경우
    if (nextFront <= copyRear)
    {
        directSize = copyRear - nextFront + 1;
    }
    else
    {
        directSize = (RING_BUFFER_SIZE - 1) - nextFront + 1;
    }

    return directSize;
}
