#include "BotManager.h"

void BotManager::Initialize(const BotConfig& cfg, JobDispatcher* dispatcher)
{
	m_cfg = cfg;
	m_dispatcher = dispatcher;
	m_bots.resize(cfg.botCount);
	for (int32_t i = 0; i < cfg.botCount; ++i)
	{
		m_bots[i].slot = i;
	}
}

void BotManager::Start()
{
	LOG_INFO("Starting % bots (scenario=%, packets/bot=%, interval=%ms)",
		m_cfg.botCount, static_cast<int32_t>(m_cfg.scenario), m_cfg.packetsPerBot, m_cfg.packetIntervalMs);

	m_startTime = std::chrono::steady_clock::now();
	m_nextStatsTime = m_startTime + std::chrono::seconds(1);

	auto now = m_startTime;
	for (int32_t i = 0; i < m_cfg.botCount; ++i)
	{
		NetworkServer::GetInstance()->Connect("127.0.0.1", 7777, m_dispatcher);
		m_bots[i].state = BotState::Connecting;
		m_bots[i].nextSendTime = now;
	}
}

void BotManager::TryAssignSessionIDs()
{
	for (auto& bot : m_bots)
	{
		if (bot.state != BotState::Connecting)
		{
			continue;
		}

		//다음 미할당 sessionID 폴링
		auto session = NetworkServer::GetInstance()->FindSession(m_lastAssignedID + 1);
		if (session == nullptr)
		{
			break;  //순차 발급되므로 더 볼 필요 없음
		}

		bot.sessionID = m_lastAssignedID + 1;
		bot.state = BotState::Connected;
		m_lastAssignedID++;
	}
}

bool BotManager::SendPacket(Bot& bot)
{
	auto session = NetworkServer::GetInstance()->FindSession(bot.sessionID);
	if (session == nullptr)
	{
		return false;
	}

	CLGS_AUTHEN_REQT req;
	req.seq = bot.packetsSent + 1;
	req.accounttoken = "dummy_token";
	req.reconnect = false;
	req.connectSessionKey = "";

	session->Send(req);
	return true;
}

void BotManager::DisconnectBot(Bot& bot)
{
	if (bot.state != BotState::Connected)
	{
		return;
	}

	NetworkServer::GetInstance()->RemoveSession(bot.sessionID);
	bot.sessionID = 0;
	bot.state = BotState::Disconnected;
	bot.nextActionTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(m_cfg.reconnectDelayMs);
	m_totalDisconnects++;
}

void BotManager::ReconnectBot(Bot& bot)
{
	NetworkServer::GetInstance()->Connect("127.0.0.1", 7777, m_dispatcher);
	bot.state = BotState::Connecting;
	bot.packetsSent = 0;  //새 세션에서 카운트 다시 시작
	bot.reconnectCount++;
	m_totalReconnects++;
}

bool BotManager::ShouldThisBotReconnect(int32_t slot) const
{
	//slot 기반 결정적 비율 — 매 호출 동일 결과
	return (slot * 100 / std::max(m_cfg.botCount, 1)) < m_cfg.reconnectRatePct;
}

void BotManager::Tick()
{
	auto now = std::chrono::steady_clock::now();

	TryAssignSessionIDs();

	for (auto& bot : m_bots)
	{
		if (bot.state == BotState::Connected)
		{
			//패킷 송신
			if (bot.packetsSent < m_cfg.packetsPerBot && now >= bot.nextSendTime)
			{
				if (SendPacket(bot))
				{
					bot.packetsSent++;
					bot.totalPacketsSent++;
					m_totalSent++;
				}
				else
				{
					m_totalSendErrors++;
				}
				bot.nextSendTime = now + std::chrono::milliseconds(m_cfg.packetIntervalMs);
			}

			//시나리오별 끊기 트리거
			if (bot.packetsSent >= m_cfg.packetsPerBot)
			{
				if (m_cfg.scenario == BotScenario::DisconnectOnly)
				{
					DisconnectBot(bot);  //전부 끊기, 재접속 X
				}
				else if (m_cfg.scenario == BotScenario::Reconnect && ShouldThisBotReconnect(bot.slot))
				{
					DisconnectBot(bot);  //일부만 끊고 재접속 큐
				}
			}
		}
		else if (bot.state == BotState::Disconnected)
		{
			if (m_cfg.scenario == BotScenario::Reconnect && now >= bot.nextActionTime)
			{
				ReconnectBot(bot);
			}
		}
	}

	//실시간 통계
	if (now >= m_nextStatsTime)
	{
		PrintLiveStats();
		m_nextStatsTime = now + std::chrono::seconds(1);
	}
}

bool BotManager::AllDone() const
{
	for (const auto& bot : m_bots)
	{
		if (bot.state == BotState::Connecting)
		{
			return false;
		}
		if (bot.state == BotState::Connected && bot.packetsSent < m_cfg.packetsPerBot)
		{
			return false;
		}
		//재접속 시나리오: Disconnected 상태인 봇이 재접속 예정인지 확인
		if (m_cfg.scenario == BotScenario::Reconnect &&
			bot.state == BotState::Disconnected &&
			ShouldThisBotReconnect(bot.slot) &&
			bot.reconnectCount < 1)  //최소 1회 재접속 후 종료
		{
			return false;
		}
	}
	return true;
}

void BotManager::PrintLiveStats()
{
	int32_t connected = 0;
	int32_t connecting = 0;
	int32_t disconnected = 0;
	for (const auto& bot : m_bots)
	{
		if (bot.state == BotState::Connected) connected++;
		else if (bot.state == BotState::Connecting) connecting++;
		else disconnected++;
	}

	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::steady_clock::now() - m_startTime).count();

	LOG_INFO("[%sec] connected=%/%/% sent=% disc=% reco=% err=%",
		static_cast<int32_t>(elapsed),
		connected, connecting, disconnected,
		m_totalSent, m_totalDisconnects, m_totalReconnects, m_totalSendErrors);
}

void BotManager::PrintSummary()
{
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - m_startTime).count();
	double seconds = elapsed / 1000.0;

	LOG_INFO("=== SUMMARY ===");
	LOG_INFO("Elapsed: % ms", static_cast<int64_t>(elapsed));
	LOG_INFO("Bots: %", m_cfg.botCount);
	LOG_INFO("Total sent: %", m_totalSent);
	LOG_INFO("Total disconnects: %", m_totalDisconnects);
	LOG_INFO("Total reconnects: %", m_totalReconnects);
	LOG_INFO("Send errors: %", m_totalSendErrors);
	if (seconds > 0)
	{
		LOG_INFO("Throughput: % pkt/sec", static_cast<int64_t>(m_totalSent / seconds));
	}
}
