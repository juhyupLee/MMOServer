#include "SerializeBuffer.h"

#define DEBUG_BUFFER_CLEAR  0
#define PREV_DATA_MEMORY_HISTORY  0

MemoryPool_TLS<LanPacket> LanPacket::m_MemoryPool(500);
MemoryPool_TLS<NetPacket> NetPacket::m_NetPacketMemoryPool(100);

//=======================================================
// LanPacket Function
//=======================================================
LanPacket::LanPacket()
    :
    m_PayloadFreeSize(BUFFER_SIZE - sizeof(LanHeader)),
    m_WritePos(0),
    m_ReadPos(0),
    m_Buffer{0,},
    m_PayloadSize(0),
    m_RefCount(1)
{
    m_PayloadPtr = m_Buffer + sizeof(LanHeader);
    m_bSetHeader = false;
}

LanPacket::~LanPacket()
{
}

void LanPacket::Release()
{
}

void LanPacket::Clear()
{
    m_PayloadFreeSize = BUFFER_SIZE - sizeof(LanHeader);
    m_ReadPos = 0;
    m_WritePos = 0;
    m_PayloadSize = 0;
    m_bSetHeader = false;

    //------------------------------------------------
    // Reference Count 초기화해줘야됨 이거때문에 실수나옴
    //------------------------------------------------
    m_RefCount = 1;

    //------------------------------------------------
    // 보통 m_Buffer 를 밀어줄 필요는 없지만, Free됬을때 확실히 메모리가
    // Clear 됬다는걸 표시하기 위해 Debugging 용으로 버퍼의 메모리를 초기화 한다.
    //------------------------------------------------
#if DEBUG_BUFFER_CLEAR == 1
    ZeroMemory(m_Buffer, BUFFER_SIZE);
#endif


}

int32_t LanPacket::GetFreeFullBufferSize()
{
    if (m_bSetHeader)
    {
        return m_PayloadFreeSize - sizeof(LanHeader);
    }
    return m_PayloadFreeSize;
}

int32_t LanPacket::GetFreePayloadBufferSize()
{
    return m_PayloadFreeSize;
}

int32_t LanPacket::GetPayloadSize()
{
    return m_PayloadSize;
}

int32_t LanPacket::GetFullPacketSize()
{
    if (m_bSetHeader)
    {
        return m_PayloadSize+sizeof(LanHeader);
    }
    
    return m_PayloadSize;
}

char* LanPacket::GetBufferPtr(void)
{
    return m_Buffer;
}

char* LanPacket::GetPayloadPtr(void)
{
    return m_Buffer+ sizeof(LanHeader);
}

int32_t LanPacket::MoveWritePos(int size)
{
    if (size < 0)
    {
        return 0;
    }
    if (m_PayloadFreeSize <= size)
    {
        size = m_PayloadFreeSize;
    }

    m_WritePos += size;
    m_PayloadSize += size;
    m_PayloadFreeSize -= size;

    return size;
}

int32_t LanPacket::MoveReadPos(int size)
{
    if (size < 0)
    {
        return 0;
    }
    if (m_PayloadSize <= size)
    {
        size = m_PayloadSize;
    }
    m_ReadPos += size;
    m_PayloadSize -= size;
    m_PayloadFreeSize += size;
    return size;
}

//---------------------------------------------------------------------
// Data -> SerializeBuffer
//---------------------------------------------------------------------

LanPacket& LanPacket::operator<<(BYTE inValue)
{
    if (m_PayloadFreeSize < sizeof(BYTE))
    {
        throw PacketExcept(L"char: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(m_PayloadPtr+m_WritePos, &inValue, sizeof(BYTE));

    m_WritePos += sizeof(BYTE);
    m_PayloadSize += sizeof(BYTE);
    m_PayloadFreeSize -= sizeof(BYTE);

    return *this;
}

LanPacket& LanPacket::operator<<(char inValue)
{
    if (m_PayloadFreeSize < sizeof(char))
    {
        throw PacketExcept(L"char: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }
    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(char));

    m_WritePos += sizeof(char);
    m_PayloadSize += sizeof(char);
    m_PayloadFreeSize -= sizeof(char);

    return *this;

}

LanPacket& LanPacket::operator<<(short inValue)
{
    if (m_PayloadFreeSize < sizeof(short))
    {
        throw PacketExcept(L"short: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }
    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(short));

    m_WritePos += sizeof(short);
    m_PayloadSize += sizeof(short);
    m_PayloadFreeSize -= sizeof(short);

    return *this;
}

LanPacket& LanPacket::operator<<(WORD inValue)
{
    if (m_PayloadFreeSize < sizeof(WORD))
    {
        throw PacketExcept(L"WORD: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }
    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(WORD));

    m_WritePos += sizeof(WORD);
    m_PayloadSize += sizeof(WORD);
    m_PayloadFreeSize -= sizeof(WORD);

    return *this;
}

LanPacket& LanPacket::operator<<(int32_t inValue)
{
    if (m_PayloadFreeSize <sizeof(int32_t))
    {
        throw PacketExcept(L"int32: 여유공간이 없습니다.",L"SerilizeBuffer.cpp",__LINE__);
    }

    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(int32_t));

    m_WritePos += sizeof(int32_t);
    m_PayloadSize += sizeof(int32_t);
    m_PayloadFreeSize -= sizeof(int32_t);

    return *this;
}

LanPacket& LanPacket::operator<<(DWORD inValue)
{
    if (m_PayloadFreeSize < sizeof(DWORD))
    {
        throw PacketExcept(L"DWORD: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(DWORD));

    m_WritePos += sizeof(DWORD);
    m_PayloadSize += sizeof(DWORD);
    m_PayloadFreeSize -= sizeof(DWORD);

    return *this;
}

LanPacket& LanPacket::operator<<(float inValue)
{
    if (m_PayloadFreeSize < sizeof(float))
    {
        throw PacketExcept(L"float: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(float));

    m_WritePos += sizeof(float);
    m_PayloadSize += sizeof(float);
    m_PayloadFreeSize -= sizeof(float);

    return *this;
}

LanPacket& LanPacket::operator<<(int64_t inValue)
{
    if (m_PayloadFreeSize < sizeof(int64_t))
    {
        throw PacketExcept(L"int64_t: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(int64_t));

    m_WritePos += sizeof(int64_t);
    m_PayloadSize += sizeof(int64_t);
    m_PayloadFreeSize -= sizeof(int64_t);

    return *this;
}

LanPacket& LanPacket::operator<<(double inValue)
{
    if (m_PayloadFreeSize < sizeof(double))
    {
        throw PacketExcept(L"double: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(double));

    m_WritePos += sizeof(double);
    m_PayloadSize += sizeof(double);
    m_PayloadFreeSize -= sizeof(double);

    return *this;
}

//---------------------------------------------------------------------
// SerializeBuffer-->Data 
//---------------------------------------------------------------------
LanPacket& LanPacket::operator>>(BYTE& outValue)
{
    //----------------------------------
   // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
   // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
   //----------------------------------
    if (m_PayloadSize < sizeof(BYTE))
    {
        throw PacketExcept(L"char: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }
    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(BYTE));

    m_ReadPos += sizeof(BYTE);
    m_PayloadSize -= sizeof(BYTE);
    m_PayloadFreeSize += sizeof(BYTE);

    return *this;
}

LanPacket& LanPacket::operator>>(char& outValue)
{
    //----------------------------------
  // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
  // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
  //----------------------------------
    if (m_PayloadSize < sizeof(char))
    {
        throw PacketExcept(L"char: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(char));

    m_ReadPos += sizeof(char);
    m_PayloadSize -= sizeof(char);
    m_PayloadFreeSize += sizeof(char);

    return *this;
}

LanPacket& LanPacket::operator>>(short& outValue)
{
    //----------------------------------
  // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
  // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
  //----------------------------------
    if (m_PayloadSize < sizeof(short))
    {
        throw PacketExcept(L"short: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(short));

    m_ReadPos += sizeof(short);
    m_PayloadSize -= sizeof(short);
    m_PayloadFreeSize += sizeof(short);

    return *this;
}

LanPacket& LanPacket::operator>>(WORD& outValue)
{
    //----------------------------------
 // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
 // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
 //----------------------------------
    if (m_PayloadSize < sizeof(WORD))
    {
        throw PacketExcept(L"WORD: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(WORD));

    m_ReadPos += sizeof(WORD);
    m_PayloadSize -= sizeof(WORD);
    m_PayloadFreeSize += sizeof(WORD);

    return *this;
}

LanPacket& LanPacket::operator>>(int32_t& outValue)
{
    
    //----------------------------------
    // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
    // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
    //----------------------------------
    if (m_PayloadSize < sizeof(int32_t))
    {
        throw PacketExcept(L"int32_t: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos,sizeof(int32_t));

    m_ReadPos += sizeof(int32_t);
    m_PayloadSize -= sizeof(int32_t);
    m_PayloadFreeSize += sizeof(int32_t);

    return *this;
}

LanPacket& LanPacket::operator>>(DWORD& outValue)
{

    //----------------------------------
    // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
    // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
    //----------------------------------
    if (m_PayloadSize < sizeof(DWORD))
    {
        throw PacketExcept(L"DWORD: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(DWORD));

    m_ReadPos += sizeof(DWORD);
    m_PayloadSize -= sizeof(DWORD);
    m_PayloadFreeSize += sizeof(DWORD);

    return *this;
}

LanPacket& LanPacket::operator>>(float& outValue)
{
    //----------------------------------
  // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
  // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
  //----------------------------------
    if (m_PayloadSize < sizeof(float))
    {
        throw PacketExcept(L"float: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(float));

    m_ReadPos += sizeof(float);
    m_PayloadSize -= sizeof(float);
    m_PayloadFreeSize += sizeof(float);

    return *this;
}

LanPacket& LanPacket::operator>>(int64_t& outValue)
{
    //----------------------------------
// 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
// 그렇지 않다면 로그를 남기거나 종료를 시킨다.
//----------------------------------
    if (m_PayloadSize < sizeof(int64_t))
    {
        throw PacketExcept(L"int64_t: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(int64_t));

    m_ReadPos += sizeof(int64_t);
    m_PayloadSize -= sizeof(int64_t);
    m_PayloadFreeSize += sizeof(int64_t);

    return *this;
}

LanPacket& LanPacket::operator>>(double& outValue)
{
    //----------------------------------
// 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
// 그렇지 않다면 로그를 남기거나 종료를 시킨다.
//----------------------------------
    if (m_PayloadSize < sizeof(double))
    {
        throw PacketExcept(L"double: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(double));

    m_ReadPos += sizeof(double);
    m_PayloadSize -= sizeof(double);
    m_PayloadFreeSize += sizeof(double);

    return *this;
}

int32_t LanPacket::GetData(char* dest, int size)
{
    if (m_PayloadSize <= size)
    {
        size = m_PayloadSize;
    }

    memcpy(dest, m_PayloadPtr + m_ReadPos, size);
    m_ReadPos += size;
    m_PayloadSize -= size;
    m_PayloadFreeSize += size;

    return size;
}

int32_t LanPacket::PutData(char* src, int size)
{
    if (m_PayloadFreeSize < size)
    {
        size = m_PayloadFreeSize;
    }

    memcpy(m_PayloadPtr + m_WritePos,src, size);

    m_WritePos += size;
    m_PayloadSize += size;
    m_PayloadFreeSize -= size;
    return size;
}

int32_t LanPacket::IncrementRefCount()
{
    return InterlockedIncrement((LONG*)&m_RefCount);
}

int32_t LanPacket::DecrementRefCount()
{
    return InterlockedDecrement((LONG*)&m_RefCount);
}

int32_t LanPacket::GetMemoryPoolAllocCount()
{
    //return m_MemoryPool.GetAllocCount();
    return m_MemoryPool.GetChunkCount();
}

LanPacket* LanPacket::Alloc()
{
    //------------------------------------
    // Alloc에서 메모리를 초기화 할경우, 
    // Free때 이전에 데이터 사용흔적이있는채로 
    // 그대로 Free가 된다.
    //------------------------------------
#if PREV_DATA_MEMORY_HISTORY ==1 
    LanPacket* rtnPacket = m_MemoryPool.Alloc();
    rtnPacket->Clear();
    return rtnPacket;
#endif 

#if PREV_DATA_MEMORY_HISTORY ==0
    return m_MemoryPool.Alloc();
#endif 
}

void LanPacket::Free(LanPacket* packet)
{
    //------------------------------------
    // Alloc에서 메모리를 초기화 할경우, 
    // Free때 이전에 데이터 사용흔적이있는채로 
    // 그대로 Free가 된다.
    //------------------------------------

#if PREV_DATA_MEMORY_HISTORY ==1
#endif 

#if PREV_DATA_MEMORY_HISTORY ==0
    packet->Clear();
#endif 
    m_MemoryPool.Free(packet);
 
}

void LanPacket::SetHeader(LanHeader* header)
{
    m_bSetHeader = true;
    memcpy(m_Buffer, header, sizeof(LanHeader));
}

//=======================================================
// NetPacket Function
//=======================================================
NetPacket::NetPacket()
    :
    m_PayloadFreeSize(BUFFER_SIZE - sizeof(NetHeader)),
    m_WritePos(0),
    m_ReadPos(0),
    m_Buffer{ 0, },
    m_PayloadSize(0),
    m_RefCount(1),
    m_bEencode(false),
    m_bSetHeader(false)
{
    m_PayloadPtr = m_Buffer + sizeof(NetHeader);
}

NetPacket::~NetPacket()
{
}

void NetPacket::Release()
{
}

void NetPacket::Clear()
{
    m_PayloadFreeSize = BUFFER_SIZE - sizeof(NetHeader);
    m_ReadPos = 0;
    m_WritePos = 0;
    m_PayloadSize = 0;
    m_bSetHeader = false;
    m_bEencode = false;
    m_RefCount = 1;

}

int32_t NetPacket::GetFreeFullBufferSize()
{
    if (m_bSetHeader)
    {
        return m_PayloadFreeSize - sizeof(NetHeader);
    }
    return m_PayloadFreeSize;
}

int32_t NetPacket::GetFreePayloadBufferSize()
{
    return m_PayloadFreeSize;
}

int32_t NetPacket::GetPayloadSize()
{
    return m_PayloadSize;
}

int32_t NetPacket::GetFullPacketSize()
{
    if (m_bSetHeader)
    {
        return m_PayloadSize + sizeof(NetHeader);
    }

    return m_PayloadSize;
}

char* NetPacket::GetBufferPtr(void)
{
    return m_Buffer;
}

char* NetPacket::GetPayloadPtr(void)
{
    return m_Buffer + sizeof(NetHeader);
}

int32_t NetPacket::MoveWritePos(int size)
{
    if (size < 0)
    {
        return 0;
    }
    if (m_PayloadFreeSize <= size)
    {
        size = m_PayloadFreeSize;
    }

    m_WritePos += size;
    m_PayloadSize += size;
    m_PayloadFreeSize -= size;

    return size;
}

int32_t NetPacket::MoveReadPos(int size)
{
    if (size < 0)
    {
        return 0;
    }
    if (m_PayloadSize <= size)
    {
        size = m_PayloadSize;
    }
    m_ReadPos += size;
    m_PayloadSize -= size;
    m_PayloadFreeSize += size;
    return size;
}

//---------------------------------------------------------------------
// Data -> SerializeBuffer
//---------------------------------------------------------------------

NetPacket& NetPacket::operator<<(BYTE inValue)
{
    if (m_PayloadFreeSize < sizeof(BYTE))
    {
        throw PacketExcept(L"char: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(BYTE));

    m_WritePos += sizeof(BYTE);
    m_PayloadSize += sizeof(BYTE);
    m_PayloadFreeSize -= sizeof(BYTE);

    return *this;
}

NetPacket& NetPacket::operator<<(char inValue)
{
    if (m_PayloadFreeSize < sizeof(char))
    {
        throw PacketExcept(L"char: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }
    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(char));

    m_WritePos += sizeof(char);
    m_PayloadSize += sizeof(char);
    m_PayloadFreeSize -= sizeof(char);

    return *this;

}

NetPacket& NetPacket::operator<<(short inValue)
{
    if (m_PayloadFreeSize < sizeof(short))
    {
        throw PacketExcept(L"short: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }
    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(short));

    m_WritePos += sizeof(short);
    m_PayloadSize += sizeof(short);
    m_PayloadFreeSize -= sizeof(short);

    return *this;
}

NetPacket& NetPacket::operator<<(WORD inValue)
{
    if (m_PayloadFreeSize < sizeof(WORD))
    {
        throw PacketExcept(L"WORD: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }
    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(WORD));

    m_WritePos += sizeof(WORD);
    m_PayloadSize += sizeof(WORD);
    m_PayloadFreeSize -= sizeof(WORD);

    return *this;
}

NetPacket& NetPacket::operator<<(int32_t inValue)
{
    if (m_PayloadFreeSize < sizeof(int32_t))
    {
        throw PacketExcept(L"int32: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(int32_t));

    m_WritePos += sizeof(int32_t);
    m_PayloadSize += sizeof(int32_t);
    m_PayloadFreeSize -= sizeof(int32_t);

    return *this;
}

NetPacket& NetPacket::operator<<(DWORD inValue)
{
    if (m_PayloadFreeSize < sizeof(DWORD))
    {
        throw PacketExcept(L"DWORD: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(DWORD));

    m_WritePos += sizeof(DWORD);
    m_PayloadSize += sizeof(DWORD);
    m_PayloadFreeSize -= sizeof(DWORD);

    return *this;
}

NetPacket& NetPacket::operator<<(float inValue)
{
    if (m_PayloadFreeSize < sizeof(float))
    {
        throw PacketExcept(L"float: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(float));

    m_WritePos += sizeof(float);
    m_PayloadSize += sizeof(float);
    m_PayloadFreeSize -= sizeof(float);

    return *this;
}

NetPacket& NetPacket::operator<<(int64_t inValue)
{
    if (m_PayloadFreeSize < sizeof(int64_t))
    {
        throw PacketExcept(L"int64_t: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(int64_t));

    m_WritePos += sizeof(int64_t);
    m_PayloadSize += sizeof(int64_t);
    m_PayloadFreeSize -= sizeof(int64_t);

    return *this;
}

NetPacket& NetPacket::operator<<(double inValue)
{
    if (m_PayloadFreeSize < sizeof(double))
    {
        throw PacketExcept(L"double: 여유공간이 없습니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(m_PayloadPtr + m_WritePos, &inValue, sizeof(double));

    m_WritePos += sizeof(double);
    m_PayloadSize += sizeof(double);
    m_PayloadFreeSize -= sizeof(double);

    return *this;
}

//---------------------------------------------------------------------
// SerializeBuffer-->Data 
//---------------------------------------------------------------------
NetPacket& NetPacket::operator>>(BYTE& outValue)
{
    //----------------------------------
   // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
   // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
   //----------------------------------
    if (m_PayloadSize < sizeof(BYTE))
    {
        throw PacketExcept(L"char: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }
    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(BYTE));

    m_ReadPos += sizeof(BYTE);
    m_PayloadSize -= sizeof(BYTE);
    m_PayloadFreeSize += sizeof(BYTE);

    return *this;
}

NetPacket& NetPacket::operator>>(char& outValue)
{
    //----------------------------------
  // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
  // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
  //----------------------------------
    if (m_PayloadSize < sizeof(char))
    {
        throw PacketExcept(L"char: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(char));

    m_ReadPos += sizeof(char);
    m_PayloadSize -= sizeof(char);
    m_PayloadFreeSize += sizeof(char);

    return *this;
}

NetPacket& NetPacket::operator>>(short& outValue)
{
    //----------------------------------
  // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
  // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
  //----------------------------------
    if (m_PayloadSize < sizeof(short))
    {
        throw PacketExcept(L"short: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(short));

    m_ReadPos += sizeof(short);
    m_PayloadSize -= sizeof(short);
    m_PayloadFreeSize += sizeof(short);

    return *this;
}

NetPacket& NetPacket::operator>>(WORD& outValue)
{
    //----------------------------------
 // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
 // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
 //----------------------------------
    if (m_PayloadSize < sizeof(WORD))
    {
        throw PacketExcept(L"WORD: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(WORD));

    m_ReadPos += sizeof(WORD);
    m_PayloadSize -= sizeof(WORD);
    m_PayloadFreeSize += sizeof(WORD);

    return *this;
}

NetPacket& NetPacket::operator>>(int32_t& outValue)
{

    //----------------------------------
    // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
    // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
    //----------------------------------
    if (m_PayloadSize < sizeof(int32_t))
    {
        throw PacketExcept(L"int32_t: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(int32_t));

    m_ReadPos += sizeof(int32_t);
    m_PayloadSize -= sizeof(int32_t);
    m_PayloadFreeSize += sizeof(int32_t);

    return *this;
}

NetPacket& NetPacket::operator>>(DWORD& outValue)
{

    //----------------------------------
    // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
    // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
    //----------------------------------
    if (m_PayloadSize < sizeof(DWORD))
    {
        throw PacketExcept(L"DWORD: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(DWORD));

    m_ReadPos += sizeof(DWORD);
    m_PayloadSize -= sizeof(DWORD);
    m_PayloadFreeSize += sizeof(DWORD);

    return *this;
}

NetPacket& NetPacket::operator>>(float& outValue)
{
    //----------------------------------
  // 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
  // 그렇지 않다면 로그를 남기거나 종료를 시킨다.
  //----------------------------------
    if (m_PayloadSize < sizeof(float))
    {
        throw PacketExcept(L"float: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(float));

    m_ReadPos += sizeof(float);
    m_PayloadSize -= sizeof(float);
    m_PayloadFreeSize += sizeof(float);

    return *this;
}

NetPacket& NetPacket::operator>>(int64_t& outValue)
{
    //----------------------------------
// 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
// 그렇지 않다면 로그를 남기거나 종료를 시킨다.
//----------------------------------
    if (m_PayloadSize < sizeof(int64_t))
    {
        throw PacketExcept(L"int64_t: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(int64_t));

    m_ReadPos += sizeof(int64_t);
    m_PayloadSize -= sizeof(int64_t);
    m_PayloadFreeSize += sizeof(int64_t);

    return *this;
}

NetPacket& NetPacket::operator>>(double& outValue)
{
    //----------------------------------
// 버퍼에있는 데이터가 0보다 커야 버퍼에서뽑을 수 있다.
// 그렇지 않다면 로그를 남기거나 종료를 시킨다.
//----------------------------------
    if (m_PayloadSize < sizeof(double))
    {
        throw PacketExcept(L"double: 사용중인 공간이 부족합니다.", L"SerilizeBuffer.cpp", __LINE__);
    }

    memcpy(&outValue, m_PayloadPtr + m_ReadPos, sizeof(double));

    m_ReadPos += sizeof(double);
    m_PayloadSize -= sizeof(double);
    m_PayloadFreeSize += sizeof(double);

    return *this;
}

int32_t NetPacket::GetData(char* dest, int size)
{
    if (m_PayloadSize <= size)
    {
        size = m_PayloadSize;
    }

    memcpy(dest, m_PayloadPtr + m_ReadPos, size);
    m_ReadPos += size;
    m_PayloadSize -= size;
    m_PayloadFreeSize += size;

    return size;
}

int32_t NetPacket::PutData(char* src, int size)
{
    if (m_PayloadFreeSize < size)
    {
        size = m_PayloadFreeSize;
    }

    memcpy(m_PayloadPtr + m_WritePos, src, size);

    m_WritePos += size;
    m_PayloadSize += size;
    m_PayloadFreeSize -= size;
    return size;
}

int32_t NetPacket::IncrementRefCount()
{
    return InterlockedIncrement((LONG*)&m_RefCount);
}

int32_t NetPacket::DecrementRefCount()
{
    return InterlockedDecrement((LONG*)&m_RefCount);
}

int32_t NetPacket::GetMemoryPoolAllocCount()
{
    return m_NetPacketMemoryPool.GetChunkCount();
}

int32_t NetPacket::GetPoolCount()
{
    return  m_NetPacketMemoryPool.GetPoolCount();
}

int32_t NetPacket::GetUseCount()
{
    return m_NetPacketMemoryPool.GetUseCount();
}

NetPacket* NetPacket::Alloc()
{
    //------------------------------------
  // Alloc에서 메모리를 초기화 할경우, 
  // Free때 이전에 데이터 사용흔적이있는채로 
  // 그대로 Free가 된다.
  //------------------------------------
#if PREV_DATA_MEMORY_HISTORY ==1 
    NetPacket* rtnPacket = m_MemoryPool.Alloc();
    rtnPacket->Clear();
    return rtnPacket;
#endif 

#if PREV_DATA_MEMORY_HISTORY ==0
    return m_NetPacketMemoryPool.Alloc();
#endif 
}

void NetPacket::Free(NetPacket* packet)
{
    //------------------------------------
   // Alloc에서 메모리를 초기화 할경우, 
   // Free때 이전에 데이터 사용흔적이있는채로 
   // 그대로 Free가 된다.
   //------------------------------------

#if PREV_DATA_MEMORY_HISTORY ==1
#endif 

#if PREV_DATA_MEMORY_HISTORY ==0
    packet->Clear();
#endif 
    m_NetPacketMemoryPool.Free(packet);
}

BYTE NetPacket::GetCheckSum()
{
    BYTE sum = 0;
    BYTE* payloadPtr = (BYTE*)m_PayloadPtr;

    for (int i = 0; i < m_PayloadSize; ++i)
    {
        sum += *payloadPtr;
        ++payloadPtr;
    }
    return sum % 256;
}

bool NetPacket::Decoding(NetHeader* header)
{
    BYTE randKey = 0;
    BYTE pData = 0;
    BYTE eData = 0;
    BYTE ePrevData = 0;
    BYTE pPrevData = 0;

    randKey = header->_RandKey;

    //-------------------------------------------------
    // Check Sum  복호화
    //-------------------------------------------------
    char* decodeDataPtr = (char*)&header->_CheckSum;
    eData = *decodeDataPtr;
    pData = eData ^ (dfPACKET_KEY + 1);

    *decodeDataPtr = pData ^ (randKey + 1);

    decodeDataPtr = m_PayloadPtr;
    //-------------------------------------------------
    // Payload 복호화
    //-------------------------------------------------
    for (int i = 0; i < m_PayloadSize; ++i)
    {
        ePrevData = eData;
        eData = *decodeDataPtr;

        pPrevData = pData;
        pData = eData ^ (ePrevData + dfPACKET_KEY + i + 2);


        *decodeDataPtr = pData ^ (pPrevData + randKey + i + 2);
        ++decodeDataPtr;
    }

    if (header->_CheckSum != GetCheckSum())
    {
        return false;
    }

    return true;
}

bool NetPacket::HeaderSettingAndEncoding()
{
    //-------------------------------------------------------------------------------------------------------------------
     /*# 원본 데이터 바이트 단위  D1 D2 D3 D4
         ----------------------------------------------------------------------------------------------------------
         | D1 | D2 | D3 | D4 |
         ----------------------------------------------------------------------------------------------------------
         D1 ^ (RK + 1) = P1 | D2 ^ (P1 + RK + 2) = P2 | D3 ^ (P2 + RK + 3) = P3 | D4 ^ (P3 + RK + 4) = P4 |
         P1 ^ (K + 1) = E1 | P2 ^ (E1 + K + 2) = E2 | P3 ^ (E2 + K + 3) = E3 | P4 ^ (E3 + K + 4) = E4 |

         # 암호 데이터 바이트 단위  E1 E2 E3 E4
         ----------------------------------------------------------------------------------------------------------
         E1                      E2                          E3                           E4
         ----------------------------------------------------------------------------------------------------------*/
    //-------------------------------------------------------------------------------------------------------------------

    //--------------------------------------------------------------
    // 더블 체크락을 통해, 이미 인코딩된 패킷이라면, 그냥 return하고
    // 인코딩이안되어있다면. 인코딩을 1회 수행해준다.
    //--------------------------------------------------------------
    if (!m_bEencode)
    {
        m_Lock.Lock();
        if (!m_bEencode)
        {
            //-------------------------------------------------------
            // 헤더 세팅
            //-------------------------------------------------------
            NetHeader header;

            header._CheckSum = GetCheckSum();
            header._Code = dfPACKET_CODE;
            header._Len = m_PayloadSize;
            header._RandKey = rand() % 256;

            SetHeader(&header);

            BYTE randKey = 0;
            BYTE pData = 0;
            BYTE eData = 0;

            randKey = header._RandKey;

            //------------------------------------------
            // Check Sum  포인터 
            //------------------------------------------
            char* encodeDataPtr = m_Buffer + 4;


            //-------------------------------------------------
            // Check Sum  인코딩
            //-------------------------------------------------
            pData = *encodeDataPtr ^ (randKey + 1);
            eData = pData ^ (dfPACKET_KEY + 1);
            *encodeDataPtr = eData;
            ++encodeDataPtr;

            //-------------------------------------------------
            // payload  인코딩
            //-------------------------------------------------
            for (int i = 0; i < m_PayloadSize; ++i)
            {
                pData = *encodeDataPtr ^ (pData + randKey + i + 2);
                eData = pData ^ (eData + dfPACKET_KEY + i + 2);
                *encodeDataPtr = eData;
                ++encodeDataPtr;
            }
            m_bEencode = true;
        }
        m_Lock.Unlock();
    }

    return true;
}

void NetPacket::SetHeader(NetHeader* header)
{
    if (!m_bSetHeader)
    {
        m_bSetHeader = true;
        memcpy(m_Buffer, header, sizeof(NetHeader));
    }
    else
    {
        CRASH();
    }
}

SmartLanPacket::SmartLanPacket()
{
	m_Packet = nullptr;
	m_RefCount = nullptr;
}

SmartLanPacket::SmartLanPacket(LanPacket* packet)
	:m_Packet(packet)
{
	if (m_Packet != nullptr)
	{
		m_RefCount = new LONG;
		*m_RefCount = 1;
	}
	else
	{
		m_RefCount = nullptr;
		m_Packet = nullptr;
	}
}

SmartLanPacket::~SmartLanPacket()
{
    if (m_RefCount != nullptr)
    {
        if (InterlockedDecrement(m_RefCount) == 0)
        {
            
            if (*m_RefCount < 0)
            {
                int* p = nullptr;
                *p = 10;
            }

            if (m_Packet != nullptr)
            {
                m_Packet->Free(m_Packet);
            }
            delete m_RefCount;

            //wprintf(L"SmartLanPacket 소멸자 호출 mPacket[%p] m_RefCount[%p] 누수체크 :size:%u\n", m_Packet, m_RefCount, g_LeackCheck.size());		
        }


    }
}

SmartLanPacket::SmartLanPacket(const SmartLanPacket& rhs)
{
	m_Packet = rhs.m_Packet;
	m_RefCount = rhs.m_RefCount;

	if (m_RefCount != nullptr)
	{
		InterlockedIncrement(m_RefCount);
	}

}

SmartLanPacket& SmartLanPacket::operator=(const SmartLanPacket& rhs)
{
	m_Packet = rhs.m_Packet;
	m_RefCount = rhs.m_RefCount;

	if (m_RefCount != nullptr)
	{
		InterlockedIncrement(m_RefCount);
	}
	return *this;
}