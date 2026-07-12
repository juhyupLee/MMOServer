#include "../Network/JobDispatcher.h"
#include "../Network/NetworkServer.h"
#include "../Network/NetworkSession.h"

#include <boost/core/lightweight_test.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace
{
template <typename Predicate>
bool WaitUntil(Predicate&& predicate, std::chrono::milliseconds timeout)
{
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	while (std::chrono::steady_clock::now() < deadline)
	{
		if (predicate())
		{
			return true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return predicate();
}
}

int main()
{
	constexpr int senderThreadCount = 8;
	constexpr int packetsPerThread = 64;
	constexpr int expectedPacketCount = senderThreadCount * packetsPerThread;

	std::mutex receiveLock;
	std::condition_variable receiveSignal;
	std::unordered_set<std::int32_t> receivedSequences;
	std::atomic<int> duplicateCount{ 0 };

	JobDispatcher serverDispatcher(
		[&](std::int64_t, PacketHolder holder)
		{
			const auto* request = holder->message.AsCLGS_AUTHEN_REQ();
			if (request == nullptr)
			{
				return;
			}

			{
				std::lock_guard guard(receiveLock);
				if (!receivedSequences.insert(request->seq).second)
				{
					duplicateCount.fetch_add(1);
				}
			}
			receiveSignal.notify_one();
		},
		1);

	JobDispatcher clientDispatcher(
		[](std::int64_t, PacketHolder)
		{
			// The connection ACK exercises the receive/decrypt/FlatBuffers path.
		},
		1);

	auto* network = NetworkServer::GetInstance();
	BOOST_TEST(network->Initialize(2, 256 * 1024, 1024));
	const auto port = network->Listen(0, &serverDispatcher);
	BOOST_TEST(port != 0);

	network->Connect("127.0.0.1", port, &clientDispatcher);

	std::shared_ptr<NetworkSession> clientSession;
	const bool connected = WaitUntil(
		[&]()
		{
			for (std::int64_t id = 1; id <= 8; ++id)
			{
				auto candidate = network->FindSession(id);
				if (candidate != nullptr
					&& candidate->IsConnected()
					&& candidate->GetPort() == port)
				{
					clientSession = std::move(candidate);
					return true;
				}
			}
			return false;
		},
		std::chrono::seconds(5));
	BOOST_TEST(connected);

	if (clientSession != nullptr)
	{
		std::vector<std::jthread> senders;
		senders.reserve(senderThreadCount);
		for (int threadIndex = 0; threadIndex < senderThreadCount; ++threadIndex)
		{
			senders.emplace_back([clientSession, threadIndex]()
			{
				for (int packetIndex = 0; packetIndex < packetsPerThread; ++packetIndex)
				{
					CLGS_AUTHEN_REQT request;
					request.seq = threadIndex * packetsPerThread + packetIndex;
					request.accounttoken = "integration-test";
					clientSession->Send(request);
				}
			});
		}
		senders.clear(); // jthread joins before checking the received set.

		std::unique_lock lock(receiveLock);
		receiveSignal.wait_for(
			lock,
			std::chrono::seconds(10),
			[&]() { return receivedSequences.size() == expectedPacketCount; });
	}

	{
		std::lock_guard guard(receiveLock);
		BOOST_TEST_EQ(receivedSequences.size(), static_cast<std::size_t>(expectedPacketCount));
	}
	BOOST_TEST_EQ(duplicateCount.load(), 0);

	if (clientSession != nullptr)
	{
		BOOST_TEST(clientSession->Disconnect());
	}
	BOOST_TEST(WaitUntil(
		[&]() { return network->GetSessionCount() == 0; },
		std::chrono::seconds(5)));

	network->Shutdown();

	// Reuse the singleton and shut it down while connects, accepts, reads, and
	// writes are still active. Ready completion handlers may already be queued
	// on a strand when Close runs, so they must not resurrect a closed session.
	BOOST_TEST(network->Initialize(2, 256 * 1024, 1024));
	const auto shutdownRacePort = network->Listen(0, &serverDispatcher);
	BOOST_TEST(shutdownRacePort != 0);

	constexpr int activeConnectionCount = 16;
	for (int index = 0; index < activeConnectionCount; ++index)
	{
		network->Connect("127.0.0.1", shutdownRacePort, &clientDispatcher);
	}

	BOOST_TEST(WaitUntil(
		[&]()
		{
			// Outgoing sessions are inserted synchronously; one extra session
			// proves that at least one accept completion also ran.
			return network->GetSessionCount() > activeConnectionCount;
		},
		std::chrono::seconds(5)));

	std::shared_ptr<NetworkSession> shutdownRaceSession;
	BOOST_TEST(WaitUntil(
		[&]()
		{
			for (std::int64_t id = 1; id <= 4096; ++id)
			{
				auto candidate = network->FindSession(id);
				if (candidate != nullptr
					&& candidate->IsConnected()
					&& candidate->GetPort() == shutdownRacePort)
				{
					shutdownRaceSession = std::move(candidate);
					return true;
				}
			}
			return false;
		},
		std::chrono::seconds(5)));

	std::atomic<bool> keepSending{ true };
	std::atomic<std::int32_t> shutdownSequence{ 100000 };
	std::vector<std::jthread> shutdownSenders;
	if (shutdownRaceSession != nullptr)
	{
		for (int threadIndex = 0; threadIndex < senderThreadCount; ++threadIndex)
		{
			shutdownSenders.emplace_back([&]()
			{
				while (keepSending.load(std::memory_order_acquire))
				{
					CLGS_AUTHEN_REQT request;
					request.seq = shutdownSequence.fetch_add(1, std::memory_order_relaxed);
					request.accounttoken = "shutdown-race";
					shutdownRaceSession->Send(request);
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			});
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	const auto shutdownStarted = std::chrono::steady_clock::now();
	network->Shutdown();
	const auto shutdownElapsed = std::chrono::steady_clock::now() - shutdownStarted;
	keepSending.store(false, std::memory_order_release);
	shutdownSenders.clear();
	BOOST_TEST(shutdownElapsed < std::chrono::seconds(5));
	BOOST_TEST_EQ(network->GetSessionCount(), static_cast<std::size_t>(0));
	if (shutdownRaceSession != nullptr)
	{
		BOOST_TEST(!shutdownRaceSession->IsConnected());
		BOOST_TEST_EQ(shutdownRaceSession->GetPendingSendBytes(), static_cast<std::size_t>(0));
	}

	BOOST_TEST(network->Initialize(2, 256 * 1024, 1024, 4));
	std::vector<std::shared_ptr<NetworkSession>> cappedSessions;
	for (int index = 0; index < 4; ++index)
	{
		auto session = network->CreateNewSession(&clientDispatcher);
		BOOST_TEST(session != nullptr);
		cappedSessions.emplace_back(std::move(session));
	}
	BOOST_TEST(network->CreateNewSession(&clientDispatcher) == nullptr);
	BOOST_TEST_EQ(network->GetSessionCount(), static_cast<std::size_t>(4));
	network->Shutdown();
	BOOST_TEST_EQ(network->GetSessionCount(), static_cast<std::size_t>(0));

	// The accepted side's connection ACK is larger than one byte, so this
	// deterministically exercises pre-post send admission and disconnects both
	// ends without retaining a queued packet.
	BOOST_TEST(network->Initialize(2, 1, 1024, 16));
	const auto backpressurePort = network->Listen(0, &serverDispatcher);
	BOOST_TEST(backpressurePort != 0);
	network->Connect("127.0.0.1", backpressurePort, &clientDispatcher);
	BOOST_TEST(WaitUntil(
		[&]() { return network->GetSessionCount() == 0; },
		std::chrono::seconds(5)));
	network->Shutdown();

	return boost::report_errors();
}
