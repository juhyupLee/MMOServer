#include "PlayServer.h"

#include "../ServerCore/Network/NetworkServer.h"
#include "../ServerCore/Network/NetworkSession.h"
#include "../ServerCore/Utill/LogManager.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#endif

namespace
{
struct ProcessMemorySample
{
	std::uint64_t rssBytes{ 0 };
	std::uint64_t virtualBytes{ 0 };
};

ProcessMemorySample ReadProcessMemory()
{
	ProcessMemorySample result;
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS_EX counters{};
	if (GetProcessMemoryInfo(
		GetCurrentProcess(),
		reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
		sizeof(counters)))
	{
		result.rssBytes = counters.WorkingSetSize;
		result.virtualBytes = counters.PrivateUsage;
	}
#else
	std::ifstream status("/proc/self/status");
	std::string key;
	while (status >> key)
	{
		if (key == "VmRSS:" || key == "VmSize:")
		{
			std::uint64_t valueKiB = 0;
			std::string unit;
			status >> valueKiB >> unit;
			if (key == "VmRSS:")
			{
				result.rssBytes = valueKiB * 1024;
			}
			else
			{
				result.virtualBytes = valueKiB * 1024;
			}
		}
		else
		{
			std::string ignored;
			std::getline(status, ignored);
		}
	}
#endif
	return result;
}

std::uint64_t Fnv1a64(const std::string& value)
{
	std::uint64_t hash = 14695981039346656037ULL;
	for (const auto byte : value)
	{
		hash ^= static_cast<std::uint8_t>(byte);
		hash *= 1099511628211ULL;
	}
	return hash;
}

std::string Hex64(std::uint64_t value)
{
	std::ostringstream output;
	output << std::hex << std::setw(16) << std::setfill('0') << value;
	return output.str();
}

bool ValidateLoadTestChecksum(const CLGS_AUTHEN_REQT& message)
{
	static constexpr std::string_view prefix = "loadtest-fnv1a64:";
	if (!message.connectSessionKey.starts_with(prefix))
	{
		return !message.accounttoken.empty();
	}

	return !message.accounttoken.empty()
		&& message.connectSessionKey.substr(prefix.size())
			== Hex64(Fnv1a64(message.accounttoken));
}
}

PlayServer::PlayServer(PlayServerConfig config)
	: m_config(std::move(config))
	, m_main([this](int64_t key, PacketHolder message) { MainDispatch(key, message); }, 0, 2)
	, m_sub([this](int64_t key, PacketHolder message) { MainDispatch(key, message); }, 0, 1)
{
	RegisterPacket(&PlayServer::ON_CLGS_AUTHEN_REQ);
}

void PlayServer::ON_CLGS_AUTHEN_REQ(
	int64_t sessionID,
	std::shared_ptr<CLGS_AUTHEN_REQT>& msg)
{
	m_recvAuthenCount.fetch_add(1, std::memory_order_relaxed);

	CLGS_AUTHEN_ACKT ack;
	ack.seq = msg->seq;
	ack.result = static_cast<std::int32_t>(
		ValidateLoadTestChecksum(*msg)
			? EResultID::R_SUCCESS
			: EResultID::R_INVALID_DATA_FORMAT);

	if (auto session = NetworkServer::GetInstance()->FindSession(sessionID);
		session != nullptr)
	{
		session->Send(ack);
	}
}

bool PlayServer::Initialize()
{
	LogManager::GetInstance()->Init();
	m_startedAt = std::chrono::steady_clock::now();
	m_lastMetricsAt = m_startedAt;
	OpenMetricsFile();
	return NetworkServer::GetInstance()->Initialize(
		m_config.ioThreads,
		m_config.maxPendingSendBytes,
		m_config.maxPendingJobs,
		m_config.maxSessions);
}

bool PlayServer::Start()
{
	return NetworkServer::GetInstance()->Listen(m_config.port, &m_main) != 0;
}

void PlayServer::Run()
{
	LOG_INFO("Server Start");

	const auto interval = std::chrono::seconds(
		std::max<std::int32_t>(m_config.metricsIntervalSec, 1));
	auto nextMetricsAt = std::chrono::steady_clock::now() + interval;
	while (!m_stopRequested.load(std::memory_order_acquire)
		&& (m_config.externalStopSignal == nullptr
			|| *m_config.externalStopSignal == 0))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if (std::chrono::steady_clock::now() >= nextMetricsAt)
		{
			WriteMetrics();
			nextMetricsAt += interval;
		}
	}
	LOG_INFO("Server stop requested");
}

void PlayServer::Release()
{
	NetworkServer::GetInstance()->Shutdown();
	WriteMetrics(true);
	if (m_metricsStream.is_open())
	{
		m_metricsStream.close();
	}
}

void PlayServer::MainDispatch(int64_t key, PacketHolder& messageHolder)
{
	auto it = m_handlers.find(messageHolder->message.type);
	if (it != m_handlers.end())
	{
		(it->second)(key, messageHolder);
	}
}

void PlayServer::OpenMetricsFile()
{
	if (m_config.metricsFile.empty())
	{
		return;
	}

	const std::filesystem::path path(m_config.metricsFile);
	if (!path.parent_path().empty())
	{
		std::filesystem::create_directories(path.parent_path());
	}
	const bool writeHeader = !std::filesystem::exists(path)
		|| std::filesystem::file_size(path) == 0;
	m_metricsStream.open(path, std::ios::out | std::ios::app);
	if (!m_metricsStream)
	{
		LOG_ERR("Cannot open metrics file:%", m_config.metricsFile);
		return;
	}
	if (writeHeader)
	{
		m_metricsStream
			<< "run_id,timestamp_ms,elapsed_sec,final,sessions,session_objects,"
			   "created_sessions,removed_sessions,recv_tps,send_tps,recv_total,"
			   "send_total,recv_bytes,send_bytes,auth_total,protocol_errors,"
			   "network_errors,backpressure_disconnects,rss_bytes,virtual_bytes\n";
		m_metricsStream.flush();
	}
}

void PlayServer::WriteMetrics(bool finalSample)
{
	const auto now = std::chrono::steady_clock::now();
	const auto elapsed = std::chrono::duration<double>(now - m_startedAt).count();
	const auto sampleSeconds = std::max(
		std::chrono::duration<double>(now - m_lastMetricsAt).count(),
		0.001);
	auto* server = NetworkServer::GetInstance();
	const auto recv = server->GetRecvCount();
	const auto send = server->GetSendCount();
	const auto recvTps = static_cast<double>(recv - m_lastRecv) / sampleSeconds;
	const auto sendTps = static_cast<double>(send - m_lastSend) / sampleSeconds;
	const auto memory = ReadProcessMemory();
	const auto timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	LOG_INFO("[STATS] sessions=% objects=% recvTPS=% sendTPS=% totalRecv=% totalSend=% rssMB=% vmMB=%",
		static_cast<std::int64_t>(server->GetSessionCount()),
		server->GetLiveSessionObjectCount(),
		static_cast<std::int64_t>(recvTps),
		static_cast<std::int64_t>(sendTps),
		recv,
		send,
		static_cast<std::int64_t>(memory.rssBytes / 1024 / 1024),
		static_cast<std::int64_t>(memory.virtualBytes / 1024 / 1024));

	if (m_metricsStream)
	{
		m_metricsStream << m_config.runID << ',' << timestampMs << ','
			<< std::fixed << std::setprecision(3) << elapsed << ','
			<< (finalSample ? 1 : 0) << ','
			<< server->GetSessionCount() << ','
			<< server->GetLiveSessionObjectCount() << ','
			<< server->GetCreatedSessionCount() << ','
			<< server->GetRemovedSessionCount() << ','
			<< recvTps << ',' << sendTps << ','
			<< recv << ',' << send << ','
			<< server->GetRecvByteCount() << ',' << server->GetSendByteCount() << ','
			<< m_recvAuthenCount.load(std::memory_order_relaxed) << ','
			<< server->GetProtocolErrorCount() << ','
			<< server->GetNetworkErrorCount() << ','
			<< server->GetBackpressureDisconnectCount() << ','
			<< memory.rssBytes << ',' << memory.virtualBytes << '\n';
		m_metricsStream.flush();
	}

	m_lastMetricsAt = now;
	m_lastRecv = recv;
	m_lastSend = send;
}
