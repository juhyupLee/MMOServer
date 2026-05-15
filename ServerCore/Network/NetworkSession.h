#pragma once
#include <boost/asio.hpp>
#include "JobQueue.h"
#include "NetworkServer.h"

struct NetworkPacket
{
	flatbuffers::DetachedBuffer m_buffer{ };
};

class NetworkSession : public std::enable_shared_from_this<NetworkSession>
{
public:
	NetworkSession(boost::asio::io_context& ioContext, JobDispatcher* jobDispatcher);

private:
	int64_t m_sessionID;
	RingQ m_recvRingQueue;
	LockFreeQ<NetworkPacket*> m_sendQueue;
	std::shared_ptr<JobQueue> m_jobQueue;
	boost::asio::ip::tcp::socket m_socket;
	int32_t m_port;
	std::string m_ip;
	std::atomic<bool> m_isSending;

	std::vector<NetworkPacket*> m_pendingSendPackets;
	std::mutex m_sendMutex;

public:
	bool Disconnect();

	template <MessageConcept T>
	void Send(T& messsage);

	int64_t GetSessionID();

	void Connect(std::string ip, int32_t port);

	std::string GetIP();
	int32_t GetPort();
	JobDispatcher* GetJobDispatcher();

	LockFreeQ<NetworkPacket*>& GetSendQueue();
	RingQ& GetRecvRingQueue();
	void OnAccept(boost::asio::ip::tcp::socket socket, std::string ip, int32_t port);
	void OnConnected();
	void OnRecvMessage(char* messageBuffer, int32_t messageSize);

private:
	void DoReceive();
	void DoSend();
	void ProcessReceivedData(size_t bytesTransferred);
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
		//NetworkServer::GetInstance()->Convert(reinterpret_cast<char*>(packet->m_buffer.data()) + PACKET_HEADER_SIZE, messageSize);
	}

	m_sendQueue.EnQ(packet);

	boost::asio::post(m_socket.get_executor(),
		[self = shared_from_this()]()
		{
			self->DoSend();
		}
	);
}
