#pragma once

#include "../../flatbuffers/ProtocoID.h"

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <unordered_set>
#include <vector>
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
	auto messageHolder = std::make_shared<MessageHolderT>();
	messageHolder->message.Set(const_cast<T&>(message));
	return messageHolder;
}

template<typename T> concept MessageConcept = std::is_base_of_v<flatbuffers::NativeTable, T>&& MessageIDUnionTraits<T>::enum_value != MessageID::NONE;

template<MessageConcept T>
static PacketHolder CreateMessageHolder(const T& message, const std::unordered_set<int64_t>& accountIDs = {})
{
	auto messageHolder = std::make_shared<MessageHolderT>();
	messageHolder->message.Set(const_cast<T&>(message));
	std::ranges::copy(accountIDs, std::back_inserter(messageHolder->accountID));
	return messageHolder;
}
