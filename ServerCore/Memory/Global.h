#pragma once
#define dfPACKET_CODE		0x77
#define dfPACKET_KEY		0x32

constexpr int64_t MARK_FRONT = 0x1234567876543210;
constexpr int64_t MARK_REAR = 0x8765432101234567;

#define ThreadLocal __declspec(thread)

//ThreadLocal std::stack<MemoryPool_TLS<>
//ThreadLocal FreeList tls
#define YIELD_TRY_MAX 300
#define MAX_SLEEP_ITERATION 4000


//extern DWORD GMemoryPoolTLSIndex;
void CRASH();

template<typename T> concept PacketConcept = std::is_base_of_v<flatbuffers::NativeTable, T>;
template<PacketConcept T>
std::shared_ptr<MessageHolderT> CreatePacketHolder(const T& message)
{
	auto messageHolder = std::make_shared<MessageHolderT>();
	messageHolder->message.Set(const_cast<T&>(message));
	return messageHolder;
}

