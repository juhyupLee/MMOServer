#pragma once
#define dfPACKET_CODE		0x77
#define dfPACKET_KEY		0x32

void CRASH();


class MyLock
{
public:
	MyLock()
	{
		InitializeSRWLock(&m_Lock);
	}
	void Lock()
	{
		AcquireSRWLockExclusive(&m_Lock);
	}
	void Unlock()
	{
		ReleaseSRWLockExclusive(&m_Lock);
	}
private:
	SRWLOCK m_Lock;
};
template<typename T> concept PacketConcept = std::is_base_of_v<flatbuffers::NativeTable, T>;

template<PacketConcept T>
std::shared_ptr<MessageHolderT> CreatePacketHolder(const T& message)
{
	auto messageHolder = std::make_shared<MessageHolderT>();
	messageHolder->message.Set(const_cast<T&>(message));
	return messageHolder;
}