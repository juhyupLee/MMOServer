#pragma once
#include <iostream>
#include <Windows.h>
#include <locale>
//#include "FreeList.h"
#include "MemoryPool_TLS.h"

#define SMART_PACKET_USE 0

struct LanHeader;
struct NetHeader;
class PacketExcept
{
public:
	PacketExcept(const WCHAR* str, const WCHAR* file,int line)
	{
		setlocale(LC_ALL, "");
		wsprintf(m_ErrorMessage, L"%s [File:%s] [line:%d]", str,file,line);
	}

public:
	void what()
	{
		wprintf(L"%s\n", m_ErrorMessage);
	}
private:
	WCHAR m_ErrorMessage[128];

};


class LanPacket
{
private:
	//friend class FreeList< LanPacket>;
	friend class MemoryPool_TLS<LanPacket>;
	friend class LanServer;
	friend class LanClient;
	enum
	{
		BUFFER_SIZE=1000
	};
	LanPacket();
	virtual ~LanPacket();

public:
	void Release();
	void Clear();

	int32_t GetFreeFullBufferSize();
	int32_t GetFreePayloadBufferSize();
	int32_t GetPayloadSize();
	int32_t GetFullPacketSize();

	char* GetBufferPtr(void);
	char* GetPayloadPtr(void);
	int32_t MoveWritePos(int size);
	int32_t MoveReadPos(int size);

	LanPacket& operator<<(BYTE inValue);
	LanPacket& operator<<(char inValue);
	LanPacket& operator<<(short inValue);
	LanPacket& operator<<(WORD inValue);
	LanPacket& operator<<(int32_t inValue);
	LanPacket& operator<<(DWORD inValue);
	LanPacket& operator<<(float inValue);
	LanPacket& operator<<(int64_t inValue);
	LanPacket& operator<<(double inValue);

	LanPacket& operator>>(BYTE& outValue);
	LanPacket& operator>>(char& outValue);
	LanPacket& operator>>(short& outValue);
	LanPacket& operator>>(WORD& outValue);
	LanPacket& operator>>(int32_t& outValue);
	LanPacket& operator>>(DWORD& outValue);
	LanPacket& operator>>(float& outValue);
	LanPacket& operator>>(int64_t& outValue);
	LanPacket& operator>>(double& outValue);

	int32_t GetData(char* dest, int size);
	int32_t PutData(char* src, int size);

	int32_t IncrementRefCount();
	int32_t DecrementRefCount();

public:
	static int32_t GetMemoryPoolAllocCount();
	static LanPacket* Alloc();
	void Free(LanPacket* packet);
private:
	void SetHeader(LanHeader* header);
protected:
	//static FreeList<LanPacket> m_MemoryPool;
	static MemoryPool_TLS<LanPacket> m_MemoryPool;


	int32_t m_PayloadFreeSize; //헤더 제외 남은 버퍼 페이로드 사이즈
	int32_t m_PayloadSize;    //헤더 제외 순수 페이로드 사이즈
	
	bool m_bSetHeader;

	char m_Buffer[BUFFER_SIZE];
	char* m_PayloadPtr;

	int32_t m_WritePos;
	int32_t m_ReadPos;
	int32_t m_RefCount;

};

class SmartLanPacket
{
public:

	SmartLanPacket();
	SmartLanPacket(LanPacket* packet);

	~SmartLanPacket();

	SmartLanPacket(const SmartLanPacket& rhs);
	SmartLanPacket& operator =(const SmartLanPacket& rhs);

	LanPacket* GetPtr()
	{
		return m_Packet;
	}
	LanPacket& operator*()
	{
		return *m_Packet;
	}
public:
	LanPacket* m_Packet;
	LONG* m_RefCount;

};


class NetPacket
{
public:
	friend class NetServer;
	friend class MemoryPool_TLS<NetPacket>;

	enum
	{
		BUFFER_SIZE = 300
	};
	NetPacket();
	virtual ~NetPacket();

	void Release();
	void Clear();

	int32_t GetFreeFullBufferSize();
	int32_t GetFreePayloadBufferSize();
	int32_t GetPayloadSize();
	int32_t GetFullPacketSize();

	char* GetBufferPtr(void);
	char* GetPayloadPtr(void);


	int32_t MoveWritePos(int size);
	int32_t MoveReadPos(int size);

	NetPacket& operator<<(BYTE inValue);
	NetPacket& operator<<(char inValue);
	NetPacket& operator<<(short inValue);
	NetPacket& operator<<(WORD inValue);
	NetPacket& operator<<(int32_t inValue);
	NetPacket& operator<<(DWORD inValue);
	NetPacket& operator<<(float inValue);
	NetPacket& operator<<(int64_t inValue);
	NetPacket& operator<<(double inValue);

	NetPacket& operator>>(BYTE& outValue);
	NetPacket& operator>>(char& outValue);
	NetPacket& operator>>(short& outValue);
	NetPacket& operator>>(WORD& outValue);
	NetPacket& operator>>(int32_t& outValue);
	NetPacket& operator>>(DWORD& outValue);
	NetPacket& operator>>(float& outValue);
	NetPacket& operator>>(int64_t& outValue);
	NetPacket& operator>>(double& outValue);

	int32_t GetData(char* dest, int size);
	int32_t PutData(char* src, int size);

	int32_t IncrementRefCount();
	int32_t DecrementRefCount();


	static int32_t GetMemoryPoolAllocCount();
	static int32_t GetPoolCount();
	static int32_t GetUseCount();
	static NetPacket* Alloc();
	void Free(NetPacket* packet);

	//----------------------------------------------------------
	// 암호화 함수.
	//----------------------------------------------------------
	BYTE GetCheckSum();
	bool Decoding(NetHeader* header);
	bool HeaderSettingAndEncoding();

private:
	void SetHeader(NetHeader* header);
protected:
	static MemoryPool_TLS<NetPacket> m_NetPacketMemoryPool;

	int32_t m_PayloadFreeSize; //헤더 제외 남은 버퍼 페이로드 사이즈
	int32_t m_PayloadSize;    //헤더 제외 순수 페이로드 사이즈



	char m_Buffer[BUFFER_SIZE];
	char* m_PayloadPtr;

	int32_t m_WritePos;
	int32_t m_ReadPos;

	bool m_bEencode;

	MyLock m_Lock;

public:
	bool m_bSetHeader;
	int32_t m_RefCount;

};