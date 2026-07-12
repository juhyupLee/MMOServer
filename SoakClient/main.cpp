#include "../ServerCore/Dump/MemoryDump.h"
#include "../ServerCore/Network/JobDispatcher.h"
#include "../ServerCore/Network/NetworkServer.h"
#include "../ServerCore/Network/NetworkSession.h"
#include "../ServerCore/Utill/LogManager.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

volatile std::sig_atomic_t g_stopSignal = 0;

void HandleSignal(int)
{
	g_stopSignal = 1;
}

struct SoakConfig
{
	std::string host{ "127.0.0.1" };
	std::int32_t port{ 7777 };
	std::size_t botCount{ 1000 };
	std::int32_t botIDBase{ 0 };
	std::int32_t durationSec{ 24 * 60 * 60 };
	std::int32_t rampUpSec{ 60 };
	std::int32_t packetsPerSecond{ 5 };
	std::int32_t reconnectPercent{ 5 };
	std::int32_t reconnectIntervalSec{ 60 };
	std::int32_t reconnectDelayMs{ 1000 };
	std::int32_t connectTimeoutMs{ 10000 };
	std::int32_t ackTimeoutMs{ 5000 };
	std::int32_t metricsIntervalSec{ 5 };
	std::uint32_t ioThreads{ 4 };
	std::int32_t dispatcherThreads{ 4 };
	std::uint64_t seed{ 20260712 };
	std::string metricsFile{ "artifacts/client_metrics.csv" };
	std::string runID{ "local" };
	std::uint64_t preflightProbePassCount{ 0 };
	std::vector<std::size_t> payloadSizes{ 1, 32, 128, 512, 1024, 4096, 6400 };
};

enum class BotState
{
	Waiting,
	Connecting,
	Connected,
	Backoff,
};

struct Bot
{
	std::size_t slot{ 0 };
	BotState state{ BotState::Waiting };
	std::shared_ptr<NetworkSession> session;
	std::int64_t sessionID{ 0 };
	Clock::time_point nextAction{};
	Clock::time_point nextSend{};
	Clock::time_point connectStarted{};
	Clock::time_point sentAt{};
	std::int32_t pendingSequence{ 0 };
	std::size_t payloadIndex{ 0 };
	std::uint64_t generation{ 0 };
	bool awaitingAck{ false };
	bool churnRequested{ false };
};

struct PendingRequest
{
	std::size_t botSlot{ 0 };
	std::int64_t sessionID{ 0 };
	Clock::time_point sentAt{};
};

struct Counters
{
	std::uint64_t offered{ 0 };
	std::uint64_t admitted{ 0 };
	std::uint64_t acked{ 0 };
	std::uint64_t timeouts{ 0 };
	std::uint64_t unexpectedAcks{ 0 };
	std::uint64_t badAcks{ 0 };
	std::uint64_t reconnects{ 0 };
	std::uint64_t connectFailures{ 0 };
	std::uint64_t probePass{ 0 };
	std::uint64_t probeFail{ 0 };
};

constexpr std::array<double, 18> kLatencyBucketUpperMs{
	0.25, 0.5, 1.0, 2.0, 3.0, 5.0, 10.0, 20.0, 30.0,
	50.0, 75.0, 100.0, 150.0, 250.0, 500.0, 1000.0, 2000.0,
	std::numeric_limits<double>::infinity(),
};

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

std::int64_t ParseInteger(const std::string& value, const std::string& option)
{
	try
	{
		std::size_t parsed = 0;
		const auto result = std::stoll(value, &parsed);
		if (parsed != value.size())
		{
			throw std::invalid_argument("trailing characters");
		}
		return result;
	}
	catch (const std::exception&)
	{
		throw std::runtime_error("Invalid value for " + option + ": " + value);
	}
}

std::int32_t ParseDurationSeconds(const std::string& value)
{
	const auto parsed = ParseInteger(value, "--duration-sec");
	if (parsed < 0 || parsed > std::numeric_limits<std::int32_t>::max())
	{
		throw std::runtime_error(
			"--duration-sec must be 0 (unlimited) or a positive 32-bit integer");
	}
	return static_cast<std::int32_t>(parsed);
}

std::vector<std::size_t> ParsePayloadSizes(const std::string& value)
{
	std::vector<std::size_t> sizes;
	std::stringstream input(value);
	std::string token;
	while (std::getline(input, token, ','))
	{
		const auto parsed = ParseInteger(token, "--payload-sizes");
		if (parsed < 1 || parsed > 6400)
		{
			throw std::runtime_error(
				"Payload sizes must be between 1 and 6400 bytes");
		}
		sizes.push_back(static_cast<std::size_t>(parsed));
	}
	if (sizes.empty())
	{
		throw std::runtime_error("--payload-sizes cannot be empty");
	}
	return sizes;
}

SoakConfig ParseConfig(int argc, char* argv[])
{
	SoakConfig config;
	for (int index = 1; index < argc; ++index)
	{
		const std::string option = argv[index];
		auto nextValue = [&]() -> std::string
		{
			if (++index >= argc)
			{
				throw std::runtime_error("Missing value for " + option);
			}
			return argv[index];
		};

		if (option == "--host") config.host = nextValue();
		else if (option == "--port") config.port = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--bots") config.botCount = static_cast<std::size_t>(ParseInteger(nextValue(), option));
		else if (option == "--bot-id-base") config.botIDBase = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--duration-sec") config.durationSec = ParseDurationSeconds(nextValue());
		else if (option == "--ramp-up-sec") config.rampUpSec = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--pps") config.packetsPerSecond = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--reconnect-percent") config.reconnectPercent = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--reconnect-interval-sec") config.reconnectIntervalSec = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--reconnect-delay-ms") config.reconnectDelayMs = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--connect-timeout-ms") config.connectTimeoutMs = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--ack-timeout-ms") config.ackTimeoutMs = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--metrics-interval-sec") config.metricsIntervalSec = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--io-threads") config.ioThreads = static_cast<std::uint32_t>(ParseInteger(nextValue(), option));
		else if (option == "--dispatcher-threads") config.dispatcherThreads = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--seed") config.seed = static_cast<std::uint64_t>(ParseInteger(nextValue(), option));
		else if (option == "--metrics-file") config.metricsFile = nextValue();
		else if (option == "--run-id") config.runID = nextValue();
		else if (option == "--probe-pass-count") config.preflightProbePassCount = static_cast<std::uint64_t>(ParseInteger(nextValue(), option));
		else if (option == "--payload-sizes") config.payloadSizes = ParsePayloadSizes(nextValue());
		else if (option == "--help")
		{
			std::cout
				<< "SoakClient options:\n"
				<< "  --host HOST --port N --bots N --bot-id-base N\n"
				<< "  --duration-sec N (0 runs until SIGINT/SIGTERM)\n"
				<< "  --ramp-up-sec N --pps N\n"
				<< "  --reconnect-percent N --reconnect-interval-sec N\n"
				<< "  --reconnect-delay-ms N --connect-timeout-ms N --ack-timeout-ms N\n"
				<< "  --payload-sizes 1,32,128,512,1024,4096,6400\n"
				<< "  --metrics-interval-sec N --metrics-file PATH --run-id ID --seed N\n"
				<< "  --probe-pass-count N  (set by orchestration after successful preflight)\n";
			std::exit(0);
		}
		else
		{
			throw std::runtime_error("Unknown option: " + option);
		}
	}

	if (config.host.empty() || config.port <= 0 || config.port > 65535
		|| config.botCount == 0 || config.botCount > 100000
		|| config.durationSec < 0 || config.rampUpSec < 0
		|| config.packetsPerSecond <= 0 || config.packetsPerSecond > 1000
		|| config.reconnectPercent < 0 || config.reconnectPercent > 100
		|| config.reconnectIntervalSec <= 0 || config.reconnectDelayMs < 0
		|| config.connectTimeoutMs <= 0 || config.ackTimeoutMs <= 0
		|| config.metricsIntervalSec <= 0 || config.ioThreads == 0
		|| config.ioThreads > 128
		|| config.dispatcherThreads <= 0
		|| config.preflightProbePassCount > 1000)
	{
		throw std::runtime_error("SoakClient configuration is out of range");
	}
	return config;
}

class SoakRunner
{
public:
	explicit SoakRunner(SoakConfig config)
		: m_config(std::move(config))
		, m_rng(m_config.seed)
		, m_dispatcher(
			[this](std::int64_t sessionID, PacketHolder holder)
			{
				OnPacket(sessionID, std::move(holder));
			},
			0,
			m_config.dispatcherThreads)
	{
		m_counters.probePass = m_config.preflightProbePassCount;
		m_bots.resize(m_config.botCount);
		for (std::size_t index = 0; index < m_bots.size(); ++index)
		{
			m_bots[index].slot = index;
		}
	}

	int Run()
	{
		LogManager::GetInstance()->Init();
		OpenMetrics();
		if (!NetworkServer::GetInstance()->Initialize(
			m_config.ioThreads,
			256 * 1024,
			1024,
			m_config.botCount + 128))
		{
			throw std::runtime_error("NetworkServer initialization failed");
		}

		m_startedAt = Clock::now();
		m_lastMetricsAt = m_startedAt;
		m_nextChurnAt = m_startedAt
			+ std::chrono::seconds(m_config.reconnectIntervalSec);
		const auto ramp = std::chrono::milliseconds(
			static_cast<std::int64_t>(m_config.rampUpSec) * 1000);
		for (std::size_t index = 0; index < m_bots.size(); ++index)
		{
			const auto offset = m_bots.size() <= 1
				? std::chrono::milliseconds(0)
				: ramp * static_cast<std::int64_t>(index)
					/ static_cast<std::int64_t>(m_bots.size() - 1);
			m_bots[index].nextAction = m_startedAt + offset;
		}

		const auto durationLabel = m_config.durationSec == 0
			? std::string("unlimited")
			: std::to_string(m_config.durationSec) + "s";
		LOG_INFO("Soak start host=% port=% bots=% duration=% pps=% ramp=%s seed=%",
			m_config.host,
			m_config.port,
			static_cast<std::int64_t>(m_config.botCount),
			durationLabel,
			m_config.packetsPerSecond,
			m_config.rampUpSec,
			static_cast<std::int64_t>(m_config.seed));

		const auto deadline = m_config.durationSec == 0
			? Clock::time_point::max()
			: m_startedAt + std::chrono::seconds(m_config.durationSec);
		auto nextMetrics = m_startedAt
			+ std::chrono::seconds(m_config.metricsIntervalSec);
		while (!m_stopRequested.load(std::memory_order_acquire)
			&& g_stopSignal == 0 && Clock::now() < deadline)
		{
			Tick();
			if (Clock::now() >= nextMetrics)
			{
				WriteMetrics(false);
				nextMetrics += std::chrono::seconds(m_config.metricsIntervalSec);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		Drain();
		WriteMetrics(true);
		NetworkServer::GetInstance()->Shutdown();
		if (m_metrics.is_open())
		{
			m_metrics.close();
		}

		const bool passed = m_counters.timeouts == 0
			&& m_counters.unexpectedAcks == 0
			&& m_counters.badAcks == 0
			&& m_counters.probeFail == 0
			&& m_counters.offered == m_counters.admitted
			&& m_counters.admitted == m_counters.acked;
		LOG_INFO("Soak summary result=% offered=% admitted=% acked=% timeout=% badAck=% unexpectedAck=% reconnects=% connectFailures=%",
			passed ? "PASS" : "FAIL",
			static_cast<std::int64_t>(m_counters.offered),
			static_cast<std::int64_t>(m_counters.admitted),
			static_cast<std::int64_t>(m_counters.acked),
			static_cast<std::int64_t>(m_counters.timeouts),
			static_cast<std::int64_t>(m_counters.badAcks),
			static_cast<std::int64_t>(m_counters.unexpectedAcks),
			static_cast<std::int64_t>(m_counters.reconnects),
			static_cast<std::int64_t>(m_counters.connectFailures));
		return passed ? 0 : 1;
	}

	void RequestStop() noexcept
	{
		m_stopRequested.store(true, std::memory_order_release);
	}

private:
	void Tick()
	{
		const auto now = Clock::now();
		std::lock_guard guard(m_lock);

		if (now >= m_nextChurnAt && m_config.reconnectPercent > 0)
		{
			ScheduleChurn();
			m_nextChurnAt += std::chrono::seconds(m_config.reconnectIntervalSec);
		}

		for (auto& bot : m_bots)
		{
			if (bot.awaitingAck
				&& now - bot.sentAt >= std::chrono::milliseconds(m_config.ackTimeoutMs))
			{
				m_pending.erase(bot.pendingSequence);
				bot.awaitingAck = false;
				bot.pendingSequence = 0;
				++m_counters.timeouts;
				StartBackoff(bot, true);
				continue;
			}

			switch (bot.state)
			{
			case BotState::Waiting:
			case BotState::Backoff:
				if (now >= bot.nextAction)
				{
					ConnectBot(bot);
				}
				break;

			case BotState::Connecting:
				if (bot.session == nullptr
					|| NetworkServer::GetInstance()->FindSession(bot.sessionID) == nullptr
					|| now - bot.connectStarted
						>= std::chrono::milliseconds(m_config.connectTimeoutMs))
				{
					++m_counters.connectFailures;
					StartBackoff(bot, false);
				}
				break;

			case BotState::Connected:
				if (bot.session == nullptr || !bot.session->IsConnected())
				{
					++m_counters.connectFailures;
					StartBackoff(bot, false);
				}
				else if (bot.churnRequested && !bot.awaitingAck)
				{
					bot.churnRequested = false;
					StartBackoff(bot, true);
				}
				else if (!bot.awaitingAck && now >= bot.nextSend)
				{
					SendRequest(bot, now);
				}
				break;
			}
		}
	}

	void ConnectBot(Bot& bot)
	{
		auto session = NetworkServer::GetInstance()->Connect(
			m_config.host,
			m_config.port,
			&m_dispatcher);
		if (session == nullptr)
		{
			++m_counters.connectFailures;
			bot.state = BotState::Backoff;
			bot.nextAction = Clock::now() + RetryDelay();
			return;
		}

		bot.session = std::move(session);
		bot.sessionID = bot.session->GetSessionID();
		bot.connectStarted = Clock::now();
		bot.state = BotState::Connecting;
		++bot.generation;
		m_sessionToBot[bot.sessionID] = bot.slot;
	}

	void StartBackoff(Bot& bot, bool intentional)
	{
		if (bot.session != nullptr)
		{
			m_sessionToBot.erase(bot.sessionID);
			bot.session->Disconnect();
			bot.session.reset();
		}
		bot.sessionID = 0;
		bot.state = BotState::Backoff;
		bot.nextAction = Clock::now()
			+ (intentional
				? std::chrono::milliseconds(m_config.reconnectDelayMs) + Jitter()
				: RetryDelay());
		if (intentional)
		{
			++m_counters.reconnects;
		}
	}

	void SendRequest(Bot& bot, Clock::time_point now)
	{
		++m_counters.offered;
		std::int32_t sequence = static_cast<std::int32_t>(m_nextSequence++);
		if (sequence == 0)
		{
			sequence = static_cast<std::int32_t>(m_nextSequence++);
		}
		const auto payloadSize = m_config.payloadSizes[bot.payloadIndex];
		bot.payloadIndex = (bot.payloadIndex + 1) % m_config.payloadSizes.size();

		CLGS_AUTHEN_REQT request;
		request.seq = sequence;
		request.accounttoken.resize(payloadSize);
		static constexpr char alphabet[] =
			"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
		for (std::size_t index = 0; index < payloadSize; ++index)
		{
			request.accounttoken[index] = alphabet[
				(static_cast<std::uint32_t>(sequence)
					+ static_cast<std::uint32_t>(bot.slot) * 17U
					+ static_cast<std::uint32_t>(index))
				% (sizeof(alphabet) - 1)];
		}
		request.reconnect = bot.generation > 1;
		request.connectSessionKey =
			"loadtest-fnv1a64:" + Hex64(Fnv1a64(request.accounttoken));

		if (!bot.session->Send(request))
		{
			StartBackoff(bot, false);
			return;
		}

		++m_counters.admitted;
		bot.awaitingAck = true;
		bot.pendingSequence = sequence;
		bot.sentAt = now;
		bot.nextSend = now + std::chrono::microseconds(
			1000000 / m_config.packetsPerSecond);
		m_pending.emplace(
			sequence,
			PendingRequest{ bot.slot, bot.sessionID, now });
	}

	void OnPacket(std::int64_t sessionID, PacketHolder holder)
	{
		const auto now = Clock::now();
		std::lock_guard guard(m_lock);

		if (const auto* connectAck = holder->message.AsFConnectAck();
			connectAck != nullptr)
		{
			const auto found = m_sessionToBot.find(sessionID);
			if (found == m_sessionToBot.end())
			{
				++m_counters.badAcks;
				return;
			}
			auto& bot = m_bots[found->second];
			if (connectAck->result != EResultID::R_SUCCESS
				|| bot.sessionID != sessionID)
			{
				++m_counters.badAcks;
				StartBackoff(bot, false);
				return;
			}
			bot.state = BotState::Connected;
			const auto interval = std::chrono::microseconds(
				1000000 / m_config.packetsPerSecond);
			bot.nextSend = now + interval
				* static_cast<std::int64_t>(bot.slot % m_config.packetsPerSecond)
				/ m_config.packetsPerSecond;
			return;
		}

		const auto* ack = holder->message.AsCLGS_AUTHEN_ACK();
		if (ack == nullptr)
		{
			++m_counters.badAcks;
			return;
		}
		const auto pending = m_pending.find(ack->seq);
		if (pending == m_pending.end())
		{
			++m_counters.unexpectedAcks;
			return;
		}
		if (pending->second.sessionID != sessionID
			|| ack->result != static_cast<std::int32_t>(EResultID::R_SUCCESS))
		{
			++m_counters.badAcks;
		}
		const auto latencyMs = std::chrono::duration<double, std::milli>(
			now - pending->second.sentAt).count();
		RecordLatency(latencyMs);
		auto& bot = m_bots[pending->second.botSlot];
		if (bot.pendingSequence == ack->seq && bot.sessionID == sessionID)
		{
			bot.awaitingAck = false;
			bot.pendingSequence = 0;
		}
		else
		{
			++m_counters.badAcks;
		}
		m_pending.erase(pending);
		++m_counters.acked;
	}

	void RecordLatency(double milliseconds)
	{
		for (std::size_t index = 0; index < kLatencyBucketUpperMs.size(); ++index)
		{
			if (milliseconds <= kLatencyBucketUpperMs[index])
			{
				++m_latencyBuckets[index];
				return;
			}
		}
	}

	double LatencyPercentile(double percentile) const
	{
		const auto total = std::accumulate(
			m_latencyBuckets.begin(), m_latencyBuckets.end(), std::uint64_t{ 0 });
		if (total == 0)
		{
			return 0.0;
		}
		const auto target = static_cast<std::uint64_t>(
			std::ceil(static_cast<double>(total) * percentile));
		std::uint64_t cumulative = 0;
		for (std::size_t index = 0; index < m_latencyBuckets.size(); ++index)
		{
			cumulative += m_latencyBuckets[index];
			if (cumulative >= target)
			{
				return kLatencyBucketUpperMs[index];
			}
		}
		return kLatencyBucketUpperMs.back();
	}

	void ScheduleChurn()
	{
		const auto count = std::max<std::size_t>(
			1,
			m_bots.size() * static_cast<std::size_t>(m_config.reconnectPercent) / 100);
		for (std::size_t index = 0; index < count; ++index)
		{
			m_bots[m_churnCursor % m_bots.size()].churnRequested = true;
			++m_churnCursor;
		}
	}

	std::chrono::milliseconds Jitter()
	{
		std::uniform_int_distribution<std::int32_t> distribution(0, 250);
		return std::chrono::milliseconds(distribution(m_rng));
	}

	std::chrono::milliseconds RetryDelay()
	{
		return std::chrono::milliseconds(m_config.reconnectDelayMs) + Jitter();
	}

	void Drain()
	{
		const auto ackDeadline = Clock::now()
			+ std::chrono::milliseconds(m_config.ackTimeoutMs);
		while (Clock::now() < ackDeadline)
		{
			{
				std::lock_guard guard(m_lock);
				if (m_pending.empty())
				{
					break;
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		std::lock_guard guard(m_lock);
		for (auto& bot : m_bots)
		{
			if (bot.awaitingAck)
			{
				m_pending.erase(bot.pendingSequence);
				bot.awaitingAck = false;
				++m_counters.timeouts;
			}
			if (bot.session != nullptr)
			{
				bot.session->Disconnect();
				bot.session.reset();
			}
			bot.sessionID = 0;
			bot.state = BotState::Backoff;
		}
		m_sessionToBot.clear();
	}

	void OpenMetrics()
	{
		const std::filesystem::path path(m_config.metricsFile);
		if (!path.parent_path().empty())
		{
			std::filesystem::create_directories(path.parent_path());
		}
		const bool writeHeader = !std::filesystem::exists(path)
			|| std::filesystem::file_size(path) == 0;
		m_metrics.open(path, std::ios::out | std::ios::app);
		if (!m_metrics)
		{
			throw std::runtime_error("Cannot open metrics file: " + m_config.metricsFile);
		}
		if (writeHeader)
		{
			m_metrics
				<< "run_id,timestamp_ms,elapsed_sec,final,configured_bots,connected,"
				   "connecting,reconnecting,offered,admitted,acked,timeouts,"
				   "unexpected_acks,bad_acks,reconnects,connect_failures,probe_pass,"
				   "probe_fail,rtt_p50_ms,rtt_p95_ms,rtt_p99_ms,send_tps,ack_tps\n";
			m_metrics.flush();
		}
	}

	void WriteMetrics(bool finalSample)
	{
		std::lock_guard guard(m_lock);
		std::size_t connected = 0;
		std::size_t connecting = 0;
		std::size_t reconnecting = 0;
		for (const auto& bot : m_bots)
		{
			if (bot.state == BotState::Connected) ++connected;
			else if (bot.state == BotState::Connecting) ++connecting;
			else ++reconnecting;
		}

		const auto now = Clock::now();
		const auto elapsed = std::chrono::duration<double>(now - m_startedAt).count();
		const auto sampleSeconds = std::max(
			std::chrono::duration<double>(now - m_lastMetricsAt).count(), 0.001);
		const auto sendTps = static_cast<double>(
			m_counters.admitted - m_lastAdmitted) / sampleSeconds;
		const auto ackTps = static_cast<double>(
			m_counters.acked - m_lastAcked) / sampleSeconds;
		const auto timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		const auto p50 = LatencyPercentile(0.50);
		const auto p95 = LatencyPercentile(0.95);
		const auto p99 = LatencyPercentile(0.99);

		m_metrics << m_config.runID << ',' << timestampMs << ','
			<< std::fixed << std::setprecision(3) << elapsed << ','
			<< (finalSample ? 1 : 0) << ',' << m_config.botCount << ','
			<< connected << ',' << connecting << ',' << reconnecting << ','
			<< m_counters.offered << ',' << m_counters.admitted << ','
			<< m_counters.acked << ',' << m_counters.timeouts << ','
			<< m_counters.unexpectedAcks << ',' << m_counters.badAcks << ','
			<< m_counters.reconnects << ',' << m_counters.connectFailures << ','
			<< m_counters.probePass << ',' << m_counters.probeFail << ','
			<< p50 << ',' << p95 << ',' << p99 << ','
			<< sendTps << ',' << ackTps << '\n';
		m_metrics.flush();

		LOG_INFO("[SOAK] connected=% connecting=% backoff=% sendTPS=% ackTPS=% p95ms=% offered=% acked=% timeout=% bad=%",
			static_cast<std::int64_t>(connected),
			static_cast<std::int64_t>(connecting),
			static_cast<std::int64_t>(reconnecting),
			static_cast<std::int64_t>(sendTps),
			static_cast<std::int64_t>(ackTps),
			static_cast<std::int64_t>(p95),
			static_cast<std::int64_t>(m_counters.offered),
			static_cast<std::int64_t>(m_counters.acked),
			static_cast<std::int64_t>(m_counters.timeouts),
			static_cast<std::int64_t>(m_counters.badAcks + m_counters.unexpectedAcks));

		m_lastMetricsAt = now;
		m_lastAdmitted = m_counters.admitted;
		m_lastAcked = m_counters.acked;
	}

private:
	SoakConfig m_config;
	std::mutex m_lock;
	std::vector<Bot> m_bots;
	std::unordered_map<std::int64_t, std::size_t> m_sessionToBot;
	std::unordered_map<std::int32_t, PendingRequest> m_pending;
	Counters m_counters;
	std::array<std::uint64_t, kLatencyBucketUpperMs.size()> m_latencyBuckets{};
	std::mt19937_64 m_rng;
	std::atomic<bool> m_stopRequested{ false };
	std::ofstream m_metrics;
	Clock::time_point m_startedAt{};
	Clock::time_point m_lastMetricsAt{};
	Clock::time_point m_nextChurnAt{};
	std::uint64_t m_nextSequence{ 1 };
	std::uint64_t m_lastAdmitted{ 0 };
	std::uint64_t m_lastAcked{ 0 };
	std::size_t m_churnCursor{ 0 };

	// Must be destroyed first because its callback captures this.
	JobDispatcher m_dispatcher;
};
}

int main(int argc, char* argv[])
{
	CrashDump crashDump;
	std::signal(SIGINT, HandleSignal);
	std::signal(SIGTERM, HandleSignal);

	try
	{
		SoakRunner runner(ParseConfig(argc, argv));
		return runner.Run();
	}
	catch (const std::exception& exception)
	{
		std::cerr << "SoakClient fatal error: " << exception.what() << '\n';
		return 2;
	}
}
