#pragma once
#include "JobQueue.h"
#include "NetworkServer.h"
#include "NetworkTask.h"

struct NetworkTaskSend;

struct NetworkPacket
{
	flatbuffers::DetachedBuffer m_buffer{ };
};
class NetworkSession : public std::enable_shared_from_this<NetworkSession>
{
	friend struct NetworkTaskReceiveIO;
	friend struct NetworkTaskSend;

public:
	NetworkSession(JobDispatcher* jobDispatcher);
private:
	int64_t m_sessionID;
	RingQ m_recvRingQueue;
	LockFreeQ<NetworkPacket*> m_sendQueue;
	std::shared_ptr<JobQueue> m_jobQueue;
	SOCKET m_socket;
	int32_t m_port;
	std::string m_ip;
	std::atomic<bool> m_isSending;

public:

	bool Disconnect();

	template <MessageConcept T>
	void Send(T& messsage);

	int64_t GetSessionID();
	
	void Listen(int32_t port);
	void Connect(std::string ip, int32_t port);
	
	void SetSocket(SOCKET socket);
	SOCKET GetSocket();
	std::string GetIP();
	int32_t GetPort();
	JobDispatcher* GetJobDispatcher();

	LockFreeQ<NetworkPacket*>& GetSendQueue();
	RingQ& GetRecvRingQueue();
	void OnAccept(std::string ip, int32_t port, SOCKET sock);
	void OnConnected();
	void OnRecvMessage(char* messageBuffer, int32_t messageSize);
};

template <MessageConcept T>
void NetworkSession::Send(T& messsage)
{
	auto packet = new NetworkPacket;
	auto messageHolder = CreateMessageHolder(messsage);

	flatbuffers::FlatBufferBuilder fbb;
	auto offset = MessageHolder::Pack(fbb, messageHolder.get());
	fbb.FinishSizePrefixed(offset);
	packet->m_buffer = fbb.Release();
	auto messageSize = static_cast<int32_t>(packet->m_buffer.size() - PACKET_HEADER_SIZE);
	if (messageSize > 0)
	{
		NetworkServer::GetInstance()->Convert(reinterpret_cast<char*>(packet->m_buffer.data()) + PACKET_HEADER_SIZE, messageSize);
	}

	m_sendQueue.EnQ(packet);

	auto task = new NetworkTaskSend();
	task->m_owner = shared_from_this();
	NetworkServer::GetInstance()->WorkerPush(task);
}


