#include "DummyClient.h"

DummyClient::DummyClient(const BotConfig& cfg)
	: m_main([this](int64_t key, PacketHolder message) { MainDispatch(key, message); }, 1)
	, m_cfg(cfg)
{
}

bool DummyClient::Initialize()
{
	LogManager::GetInstance()->Init();
	NetworkServer::GetInstance()->Initialize();
	m_botMgr.Initialize(m_cfg, &m_main);
	return true;
}

bool DummyClient::Start()
{
	m_botMgr.Start();
	return true;
}

void DummyClient::Run()
{
	LOG_INFO("DummyClient running. Ctrl+C or Enter to stop.");

	//메인 tick 루프 — 모든 봇 시나리오 완료까지 또는 사용자 Enter
	while (!m_botMgr.AllDone())
	{
		m_botMgr.Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	//완료 후 잠시 idle (마지막 ack 받을 시간 + 통계 안정화)
	std::this_thread::sleep_for(std::chrono::seconds(1));
	m_botMgr.PrintSummary();

	LOG_INFO("Press Enter to exit.");
	std::cin.get();
}

void DummyClient::Release()
{
}

void DummyClient::MainDispatch(int64_t key, PacketHolder& messageHolder)
{
	auto it = m_handlers.find(messageHolder->message.type);
	if (it != m_handlers.end())
	{
		(it->second)(key, messageHolder);
		return;
	}
}
