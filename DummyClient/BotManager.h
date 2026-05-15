#pragma once

enum class BotScenario
{
	StaticSendRecv = 1,
	DisconnectOnly = 2,
	Reconnect      = 3,
};

struct BotConfig
{
	BotScenario scenario{ BotScenario::StaticSendRecv };
	int32_t     botCount{ 10 };
	int32_t     packetsPerBot{ 30 };
	int32_t     packetIntervalMs{ 100 };
	int32_t     reconnectDelayMs{ 1000 };  // Reconnect 시나리오에서 끊은 후 재접속까지 대기
	int32_t     reconnectRatePct{ 50 };    // Reconnect 시나리오에서 끊을 봇 비율 (%)
};

enum class BotState
{
	Connecting,
	Connected,
	Disconnected,
};

struct Bot
{
	int32_t                                slot{ 0 };          // 봇 인덱스 (0..N-1)
	int64_t                                sessionID{ 0 };     // 0이면 미할당
	int32_t                                packetsSent{ 0 };
	int32_t                                totalPacketsSent{ 0 };  // 누적 (재접속 거쳐도)
	int32_t                                reconnectCount{ 0 };
	BotState                               state{ BotState::Connecting };
	std::chrono::steady_clock::time_point  nextSendTime{ };
	std::chrono::steady_clock::time_point  nextActionTime{ };  // 재접속 시점
};

class BotManager
{
public:
	void Initialize(const BotConfig& cfg, JobDispatcher* dispatcher);
	void Start();
	void Tick();
	bool AllDone() const;

	void PrintLiveStats();
	void PrintSummary();

private:
	void TryAssignSessionIDs();
	bool SendPacket(Bot& bot);
	void DisconnectBot(Bot& bot);
	void ReconnectBot(Bot& bot);
	bool ShouldThisBotReconnect(int32_t slot) const;

	BotConfig                              m_cfg{ };
	JobDispatcher*                         m_dispatcher{ nullptr };
	std::vector<Bot>                       m_bots{ };
	int64_t                                m_lastAssignedID{ 0 };
	std::chrono::steady_clock::time_point  m_startTime{ };
	std::chrono::steady_clock::time_point  m_nextStatsTime{ };

	int64_t                                m_totalSent{ 0 };
	int64_t                                m_totalDisconnects{ 0 };
	int64_t                                m_totalReconnects{ 0 };
	int64_t                                m_totalSendErrors{ 0 };
};
