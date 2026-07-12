#pragma once

#include "../ServerCore/Network/BaseServerApp.h"
#include "../ServerCore/Network/JobDispatcher.h"

#include <atomic>
#include <cstdint>
#include <chrono>
#include <csignal>
#include <functional>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

struct PlayServerConfig
{
	std::int32_t port{ 7777 };
	std::uint32_t ioThreads{ 0 };
	std::size_t maxPendingSendBytes{ 256 * 1024 };
	std::size_t maxPendingJobs{ 256 };
	std::size_t maxSessions{ 4096 };
	std::int32_t metricsIntervalSec{ 5 };
	std::string metricsFile{};
	std::string runID{ "local" };
	const volatile std::sig_atomic_t* externalStopSignal{ nullptr };
};

class PlayServer : public BaseServerApp
{
private:
	// Declared before the dispatchers so handler storage outlives their threads.
	std::unordered_map<MessageID, std::function<void(int64_t, const PacketHolder&)>> m_handlers{ };
	PlayServerConfig m_config;
	std::atomic<bool> m_stopRequested{ false };
	std::ofstream m_metricsStream;
	std::chrono::steady_clock::time_point m_startedAt{};
	std::chrono::steady_clock::time_point m_lastMetricsAt{};
	std::int64_t m_lastRecv{ 0 };
	std::int64_t m_lastSend{ 0 };

public:
	explicit PlayServer(PlayServerConfig config = {});
	void ON_CLGS_AUTHEN_REQ(int64_t sessionID, std::shared_ptr<CLGS_AUTHEN_REQT>& msg);

	std::atomic<int64_t> m_recvAuthenCount{ 0 };
	JobDispatcher m_main;
	JobDispatcher m_sub;
	bool Initialize() override;
	bool Start() override;
	void Run() override;
	void Release() override;
	void MainDispatch(int64_t key, PacketHolder& messageHolder);
	void RequestStop() noexcept { m_stopRequested.store(true, std::memory_order_release); }

private:
	void OpenMetricsFile();
	void WriteMetrics(bool finalSample = false);

	template<MessageConcept T, auto messageID = MessageIDUnionTraits<T>::enum_value>
	void RegisterPacket(void(PlayServer::* handler)(int64_t, std::shared_ptr<T>&))
	{
		auto handlerFunc = [this, handler](int64_t sessionID, const PacketHolder& messageholder)
			{
				auto msg = std::shared_ptr<T>(static_cast<T*>(messageholder->message.value));
				messageholder->message.type = MessageID::NONE;
				(this->*handler)(sessionID, msg);
			};

		m_handlers.insert({ messageID, handlerFunc });
	}
};
