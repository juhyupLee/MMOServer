#include "RingBuffer.h"


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
    m_readPosition(0),
    m_writePosition(0)
{
}

RingQ::~RingQ()
{
}

int RingQ::Enqueue(char* buffer, int32_t size)
{

    int32_t copyFront = m_readPosition;
    int32_t copyRear = m_writePosition;

    int32_t copyFreeSize = GetFreeSize(copyFront, copyRear);

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

    copyRear = (copyRear + 1) % RING_BUFFER_SIZE;

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

    m_writePosition = copyRear;

    return size;
}

int RingQ::Dequeue(char* buffer, int32_t size)
{
    int copyRear = m_writePosition;
    int copyFront = m_readPosition;
    int copyUsedSize = GetUsedSize(copyFront, copyRear);

    if (copyUsedSize < size)
    {
        size = copyUsedSize;
        if (size <= 0)
        {
            return 0;
        }
    }

    int directSize = GetDirectDequeueSize(copyFront,copyRear);

    copyFront = (copyFront + 1) % RING_BUFFER_SIZE;

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

    m_readPosition = copyFront;

    return size;
}

int32_t RingQ::GetWriteSize() const
{
    int32_t copyReadPos = m_readPosition;
    int32_t copyWritePos = m_writePosition;

    int freeSize = 0;

    if ((copyWritePos + 1) % RING_BUFFER_SIZE == copyReadPos)
    {
        return 0;
    }
    if (copyReadPos > copyWritePos)
    {
        freeSize = copyReadPos - copyWritePos -1;
    }
    else
    {
        freeSize = copyReadPos + (RING_BUFFER_SIZE - 1) - copyWritePos;
    }
    return freeSize;
}

int32_t RingQ::GetReadSize() const
{
    int32_t copyWritePos = m_writePosition;
    int32_t copyReadPos = m_readPosition;

    int32_t readSize = 0;

    if (copyWritePos == copyReadPos)
    {
        return 0;
    }
    if (copyReadPos < copyWritePos)
    {
        readSize = copyWritePos - copyReadPos;
    }
    else
    {
        readSize = copyWritePos + (RING_BUFFER_SIZE - 1) - copyReadPos + 1;
    }
    return readSize;
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

int32_t RingQ::GetDirectWriteSize() const
{
    int copyFront = m_readPosition;
    int copyRear = m_writePosition;

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
    else
    {
        directSize = (RING_BUFFER_SIZE - 1) - rearNext + 1;
    }

    return directSize;
}

void RingQ::GetDirectEnQData(std::vector<BufferSegment>& segments)
{
    int32_t copyFront = m_readPosition;
    int32_t copyRear = m_writePosition;

    for(int32_t i = 0; i < 2; ++i)
    {
        auto buffer = GetWriteBufferPtr(copyFront, copyRear);
        if(buffer == nullptr)
        {
            break;
        }

        BufferSegment seg;
        auto directEnQSize = GetDirectEnqueueSize(copyFront, copyRear);
        seg.buf = buffer;
        seg.len = static_cast<size_t>(directEnQSize);
        segments.emplace_back(seg);

        copyRear = (copyRear + directEnQSize) % RING_BUFFER_SIZE;
    }
}

void RingQ::GetDirectDeQData(std::vector<BufferSegment>& segments)
{
    int32_t copyRear = m_writePosition;
    int32_t copyFront = m_readPosition;

    for(int i = 0; i < 2; ++i)
    {
        auto buffer = GetFrontBufferPtr(copyFront, copyRear);
        if(buffer == nullptr)
        {
            break;
        }

        int directDeQSize = GetDirectDequeueSize(copyFront, copyRear);

        BufferSegment seg;
        seg.buf = buffer;
        seg.len = static_cast<size_t>(directDeQSize);
        segments.emplace_back(seg);

        copyFront = (copyFront + directDeQSize) % RING_BUFFER_SIZE;
    }
}

int32_t RingQ::GetDirectReadSize() const
{
    int32_t copyRear = m_writePosition;
    int32_t copyFront = m_readPosition;

    if (copyFront == copyRear)
    {
        return 0;
    }

    int32_t nextFront = (copyFront + 1) % RING_BUFFER_SIZE;

    int32_t directSize = 0;
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
    m_readPosition = 0;
    m_writePosition = 0;
}

void RingQ::MoveWitePosition(int size)
{
    if (size < 0)
    {
        return;
    }

    int copyRear = m_writePosition;
    m_writePosition = (copyRear + size) % RING_BUFFER_SIZE;
}

void RingQ::MoveReadPosition(int size)
{
    if (size < 0)
    {
        return;
    }
    int copyFront = m_readPosition;
    m_readPosition = (copyFront + size ) % RING_BUFFER_SIZE;

}

char* RingQ::GetFrontBufferPtr(int32_t copyFront, int32_t copyRear)
{
    if (copyRear == copyFront)
    {
        return nullptr;
    }
    return m_Buffer + ((copyFront + 1) % RING_BUFFER_SIZE);
}

char* RingQ::GetReadBufferPtr(void)
{
    int32_t copyRear = m_writePosition;
    int32_t copyFront = m_readPosition;

    if (copyRear == copyFront)
    {
        return nullptr;
    }
    return m_Buffer + ((copyFront+1)% RING_BUFFER_SIZE);
}

char* RingQ::GetWriteBufferPtr(int32_t copyFront, int32_t copyRear)
{
    if ((copyRear + 1) % RING_BUFFER_SIZE == copyFront)
    {
        return nullptr;
    }
    return m_Buffer + ((copyRear + 1) % RING_BUFFER_SIZE);
}

char* RingQ::GetWriteBufferPtr(void)
{
    int32_t copyFront = m_readPosition;
    int32_t copyRear = m_writePosition;

    if ((copyRear +1)%RING_BUFFER_SIZE == copyFront)
    {
        return nullptr;
    }

    return m_Buffer + ((copyRear + 1) % RING_BUFFER_SIZE);
}

int RingQ::Peek(char* dest, int size)
{
    int32_t copyRear = m_writePosition;
    int32_t copyFront = m_readPosition;
    int tempUsedSize = GetUsedSize(copyFront,copyRear);

    if (tempUsedSize < size)
    {
        size = tempUsedSize;
        if (size <= 0)
        {
            return 0;
        }
    }

    int directSize = GetDirectReadSize();

    copyFront = (copyFront + 1) % RING_BUFFER_SIZE;

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

int32_t RingQ::GetReadPosition()
{
    return m_readPosition;
}

int32_t RingQ::GetwritePosition()
{
    return m_writePosition;
}

int32_t RingQ::GetDirectEnqueueSize(int32_t copyFront, int32_t copyRear) const
{
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
    else
    {
        directSize = (RING_BUFFER_SIZE - 1) - rearNext + 1;
    }

    return directSize;
}

int32_t RingQ::GetDirectDequeueSize(int32_t copyFront, int32_t copyRear) const
{
    if (copyFront == copyRear)
    {
        return 0;
    }

    int nextFront = (copyFront + 1) % RING_BUFFER_SIZE;

    int32_t directSize = 0;
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
