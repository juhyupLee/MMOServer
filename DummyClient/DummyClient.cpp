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
	LOG_INFO("DummyClient running. Press Enter to stop.");

	//별도 스레드: 키 입력 대기. Enter 누르면 stop 요청
	std::thread([this]
		{
			std::cin.get();
			m_botMgr.RequestStop();
		}).detach();

	//메인 tick 루프 — 사용자 Enter 누를 때까지 무한 반복 부하
	while (!m_botMgr.StopRequested())
	{
		m_botMgr.Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	//종료 후 마지막 통계 출력
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	m_botMgr.PrintSummary();
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
