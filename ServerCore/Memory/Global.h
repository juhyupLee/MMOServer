#pragma once
#include "MemoryPool.h"
#define YIELD_TRY_MAX 300
#define MAX_SLEEP_ITERATION 4000


using  SessionID = int64_t;
using PacketHolder = std::shared_ptr<MessageHolderT>;

constexpr int32_t	PACKET_HEADER_SIZE = sizeof(flatbuffers::uoffset_t);
constexpr int64_t MARK_FRONT = 0x1234567876543210;
constexpr int64_t MARK_REAR = 0x8765432101234567;


void CRASH();

template<typename T> concept PacketConcept = std::is_base_of_v<flatbuffers::NativeTable, T>;
template<PacketConcept T>
std::shared_ptr<MessageHolderT> CreatePacketHolder(const T& message)
{
	auto messageHolder = MakeMySharedPtr<MessageHolderT>();
	messageHolder->message.Set(const_cast<T&>(message));
	return messageHolder;
}

template<typename T> concept MessageConcept = std::is_base_of_v<flatbuffers::NativeTable, T>&& MessageIDUnionTraits<T>::enum_value != MessageID::NONE;

template<MessageConcept T>
static PacketHolder CreateMessageHolder(const T& message, const std::unordered_set<int64_t>& accountIDs = {})
{
	auto messageHolder = MakeMySharedPtr<MessageHolderT>();
	messageHolder->message.Set(const_cast<T&>(message));
	std::ranges::copy(accountIDs, std::back_inserter(messageHolder->accountID));
	return messageHolder;
}


enum class EStep
{
	None,
	Running,
	SuccessSend,
	SuccessConnect,
	Failed,
};

enum class IOType
{
	IO_NONE,
	IO_SEND,
	IO_SEND_ZERO,
	IO_RECV,
	IO_RECV_ZERO,
	IO_ACCEPT,
	IO_CONNECT,
	IO_DISCONNECT,
	IO_CONSLOE_PRINT,
	IO_FILE_WRITE
};
