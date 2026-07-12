#include "../Network/FrameDecoder.h"
#include "../Network/JobDispatcher.h"
#include "../Network/NetworkServer.h"
#include "../Network/NetworkSession.h"

#include <boost/asio.hpp>
#include <boost/core/lightweight_test.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
using namespace std::chrono_literals;

constexpr auto kWaitTimeout = 5s;
constexpr std::int32_t kTruncatedSequence = 900000;
constexpr std::int32_t kFragmentedSequence = 910000;
constexpr std::int32_t kCoalescedSequenceBase = 920000;
constexpr std::int32_t kValidBeforeMalformedSequence = 930000;
constexpr std::int32_t kForbiddenAfterMalformedSequence = 930001;
constexpr std::int32_t kSentinelSequence = 940000;

struct RoundTripState
{
	std::mutex lock;
	std::condition_variable signal;
	std::unordered_set<std::int32_t> serverReceived;
	std::unordered_set<std::int32_t> clientReceived;
	std::size_t duplicateRequests{ 0 };
	std::size_t duplicateAcks{ 0 };
	std::size_t unexpectedRequests{ 0 };
	std::size_t unexpectedAcks{ 0 };
	std::size_t payloadMismatches{ 0 };
	std::size_t ackMismatches{ 0 };
	std::size_t connectionAcks{ 0 };
	std::size_t badConnectionAcks{ 0 };
	std::size_t serverDispatches{ 0 };
	std::size_t clientDispatches{ 0 };
	std::size_t unexpectedServerMessages{ 0 };
	std::size_t unexpectedClientMessages{ 0 };
};

struct ExpectedRequest
{
	std::string accountToken;
	bool reconnect{ false };
	std::string sessionKey;
};

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
		std::this_thread::sleep_for(5ms);
	}
	return predicate();
}

std::string MakePayload(int cycle, std::size_t packetIndex, std::size_t size)
{
	std::string payload(size, ' ');
	for (std::size_t offset = 0; offset < size; ++offset)
	{
		// Printable but non-repeating test data makes truncation, corruption, and
		// packets delivered to the wrong request visible as an exact mismatch.
		payload[offset] = static_cast<char>(
			33 + ((cycle * 17 + packetIndex * 31 + offset * 7) % 90));
	}
	return payload;
}

std::vector<char> SerializeForWire(const CLGS_AUTHEN_REQT& request)
{
	auto holder = CreateMessageHolder(request);
	flatbuffers::FlatBufferBuilder builder;
	const auto offset = MessageHolder::Pack(builder, holder.get());
	builder.FinishSizePrefixed(offset);
	auto buffer = builder.Release();
	return std::vector<char>(
		reinterpret_cast<const char*>(buffer.data()),
		reinterpret_cast<const char*>(buffer.data()) + buffer.size());
}

std::vector<char> EncryptForWire(std::vector<char> frame)
{
	if (frame.size() > static_cast<std::size_t>(PACKET_HEADER_SIZE))
	{
		NetworkServer::GetInstance()->Convert(
			frame.data() + PACKET_HEADER_SIZE,
			static_cast<std::int32_t>(frame.size() - PACKET_HEADER_SIZE));
	}
	return frame;
}

std::vector<char> MakeBodyFrame(const std::vector<char>& body)
{
	std::vector<char> frame(
		static_cast<std::size_t>(PACKET_HEADER_SIZE) + body.size());
	flatbuffers::WriteScalar<flatbuffers::uoffset_t>(
		frame.data(),
		static_cast<flatbuffers::uoffset_t>(body.size()));
	std::copy(
		body.begin(),
		body.end(),
		frame.begin() + PACKET_HEADER_SIZE);
	return EncryptForWire(std::move(frame));
}

std::vector<char> MakeLengthHeader(flatbuffers::uoffset_t bodySize)
{
	std::vector<char> header(static_cast<std::size_t>(PACKET_HEADER_SIZE));
	flatbuffers::WriteScalar<flatbuffers::uoffset_t>(header.data(), bodySize);
	return header;
}

std::vector<char> SerializeMessageTypeForWire(MessageID messageType)
{
	flatbuffers::FlatBufferBuilder builder;
	const auto offset = CreateMessageHolder(builder, 0, messageType, 0);
	builder.FinishSizePrefixed(offset);
	auto buffer = builder.Release();
	std::vector<char> frame(
		reinterpret_cast<const char*>(buffer.data()),
		reinterpret_cast<const char*>(buffer.data()) + buffer.size());
	return EncryptForWire(std::move(frame));
}

bool ConnectRawSocket(
	boost::asio::ip::tcp::socket& socket,
	NetworkServer* network,
	std::uint16_t port,
	std::size_t existingSessionCount)
{
	boost::system::error_code error;
	socket.connect(
		boost::asio::ip::tcp::endpoint(
			boost::asio::ip::make_address("127.0.0.1"),
			port),
		error);
	if (error)
	{
		return false;
	}

	socket.set_option(boost::asio::ip::tcp::no_delay(true), error);
	if (error)
	{
		return false;
	}

	return WaitUntil(
		[&]()
		{
			return network->GetSessionCount() == existingSessionCount + 1;
		},
		kWaitTimeout);
}

void CloseRawSocket(boost::asio::ip::tcp::socket& socket)
{
	boost::system::error_code ignored;
	socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
	socket.close(ignored);
}

bool WriteFrameInFragments(
	boost::asio::ip::tcp::socket& socket,
	const std::vector<char>& frame)
{
	constexpr std::size_t fragmentSizes[] = {
		1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144,
	};

	std::size_t position = 0;
	std::size_t fragmentIndex = 0;
	while (position < frame.size())
	{
		const auto fragmentSize = std::min(
			fragmentSizes[fragmentIndex % std::size(fragmentSizes)],
			frame.size() - position);
		boost::system::error_code error;
		const auto written = boost::asio::write(
			socket,
			boost::asio::buffer(frame.data() + position, fragmentSize),
			error);
		if (error || written != fragmentSize)
		{
			return false;
		}
		position += fragmentSize;
		++fragmentIndex;
		std::this_thread::sleep_for(1ms);
	}
	return true;
}

bool SendRawFrameExpectDisconnect(
	NetworkServer* network,
	std::uint16_t port,
	std::size_t existingSessionCount,
	const std::vector<char>& frame)
{
	boost::asio::io_context context;
	boost::asio::ip::tcp::socket socket(context);
	if (!ConnectRawSocket(socket, network, port, existingSessionCount))
	{
		CloseRawSocket(socket);
		return false;
	}

	boost::system::error_code error;
	const auto written = boost::asio::write(socket, boost::asio::buffer(frame), error);
	const bool disconnected = WaitUntil(
		[&]() { return network->GetSessionCount() == existingSessionCount; },
		kWaitTimeout);
	CloseRawSocket(socket);
	if (!disconnected)
	{
		WaitUntil(
			[&]() { return network->GetSessionCount() == existingSessionCount; },
			kWaitTimeout);
	}
	return !error && written == frame.size() && disconnected;
}

std::shared_ptr<NetworkSession> FindConnectedClient(
	NetworkServer* network,
	std::uint16_t serverPort)
{
	// Session IDs are process-wide and monotonic. The outgoing side records the
	// listening port, while the accepted side records the client's ephemeral port.
	for (std::int64_t id = 1; id <= 65536; ++id)
	{
		auto candidate = network->FindSession(id);
		if (candidate != nullptr
			&& candidate->IsConnected()
			&& candidate->GetPort() == serverPort)
		{
			return candidate;
		}
	}
	return nullptr;
}

bool WaitForAckCount(
	RoundTripState& state,
	std::size_t expectedCount,
	std::chrono::milliseconds timeout)
{
	std::unique_lock guard(state.lock);
	return state.signal.wait_for(
		guard,
		timeout,
		[&]() { return state.clientReceived.size() >= expectedCount; });
}

bool WaitForServerCount(
	RoundTripState& state,
	std::size_t expectedCount,
	std::chrono::milliseconds timeout)
{
	std::unique_lock guard(state.lock);
	return state.signal.wait_for(
		guard,
		timeout,
		[&]() { return state.serverReceived.size() >= expectedCount; });
}

bool WaitForConnectionAckCount(
	RoundTripState& state,
	std::size_t expectedCount,
	std::chrono::milliseconds timeout)
{
	std::unique_lock guard(state.lock);
	return state.signal.wait_for(
		guard,
		timeout,
		[&]() { return state.connectionAcks >= expectedCount; });
}

bool SendTruncatedFrame(NetworkServer* network, std::uint16_t port)
{
	boost::asio::io_context context;
	boost::asio::ip::tcp::socket socket(context);
	boost::system::error_code error;
	socket.connect(
		boost::asio::ip::tcp::endpoint(
			boost::asio::ip::make_address("127.0.0.1"),
			port),
		error);
	if (error)
	{
		return false;
	}

	// Make sure the server has accepted this raw dummy connection before it is
	// terminated; otherwise a momentary zero-session count could be misleading.
	if (!WaitUntil(
		[&]() { return network->GetSessionCount() >= 1; },
		kWaitTimeout))
	{
		return false;
	}

	CLGS_AUTHEN_REQT request;
	request.seq = kTruncatedSequence;
	request.accounttoken = MakePayload(99, 0, 2048);
	auto frame = SerializeForWire(request);
	if (frame.size() <= static_cast<std::size_t>(PACKET_HEADER_SIZE + 1))
	{
		return false;
	}

	network->Convert(
		frame.data() + PACKET_HEADER_SIZE,
		static_cast<std::int32_t>(frame.size() - PACKET_HEADER_SIZE));

	const auto truncatedSize = frame.size() / 2;
	const auto bytesWritten = boost::asio::write(
		socket,
		boost::asio::buffer(frame.data(), truncatedSize),
		error);

	// Linger(0) makes this an abrupt reset, exercising cleanup of a connection
	// whose receive ring contains an incomplete frame.
	boost::asio::socket_base::linger resetOnClose(true, 0);
	boost::system::error_code ignored;
	socket.set_option(resetOnClose, ignored);
	socket.close(ignored);
	return !error && bytesWritten == truncatedSize;
}
}

int main()
{
	constexpr int reconnectCycles = 4;
	const std::vector<std::size_t> payloadSizes{
		0, 1, 2, 7, 31, 127, 511, 1023, 2047, 4095, 6144, 6800, 6900
	};

	std::unordered_map<std::int32_t, ExpectedRequest> expectedRequests;
	std::size_t minimumFrameSize = static_cast<std::size_t>(-1);
	std::size_t maximumFrameSize = 0;
	for (int cycle = 0; cycle < reconnectCycles; ++cycle)
	{
		for (std::size_t packetIndex = 0; packetIndex < payloadSizes.size(); ++packetIndex)
		{
			const auto sequence = 10000 + cycle * 1000
				+ static_cast<std::int32_t>(packetIndex);
			ExpectedRequest expected;
			expected.accountToken = MakePayload(
				cycle,
				packetIndex,
				payloadSizes[packetIndex]);
			expected.reconnect = cycle > 0;
			expected.sessionKey = "dummy-cycle-" + std::to_string(cycle);

			CLGS_AUTHEN_REQT request;
			request.seq = sequence;
			request.accounttoken = expected.accountToken;
			request.reconnect = expected.reconnect;
			request.connectSessionKey = expected.sessionKey;
			const auto frameSize = SerializeForWire(request).size();
			BOOST_TEST(frameSize <= FrameDecoder::MAX_FRAME_SIZE);
			minimumFrameSize = std::min(minimumFrameSize, frameSize);
			maximumFrameSize = std::max(maximumFrameSize, frameSize);

			expectedRequests.emplace(sequence, std::move(expected));
		}
	}

	auto addExpectedRequest = [&expectedRequests](
		std::int32_t sequence,
		std::string accountToken,
		bool reconnect,
		std::string sessionKey)
	{
		ExpectedRequest expected;
		expected.accountToken = std::move(accountToken);
		expected.reconnect = reconnect;
		expected.sessionKey = std::move(sessionKey);

		CLGS_AUTHEN_REQT request;
		request.seq = sequence;
		request.accounttoken = expected.accountToken;
		request.reconnect = expected.reconnect;
		request.connectSessionKey = expected.sessionKey;
		expectedRequests.emplace(sequence, std::move(expected));
		return request;
	};

	std::vector<CLGS_AUTHEN_REQT> rawValidRequests;
	rawValidRequests.emplace_back(addExpectedRequest(
		kFragmentedSequence,
		MakePayload(50, 0, 777),
		true,
		"raw-fragmented"));
	for (std::size_t index = 0; index < 3; ++index)
	{
		constexpr std::size_t coalescedSizes[] = { 1, 1024, 4096 };
		rawValidRequests.emplace_back(addExpectedRequest(
			kCoalescedSequenceBase + static_cast<std::int32_t>(index),
			MakePayload(51, index, coalescedSizes[index]),
			true,
			"raw-coalesced"));
	}

	const auto validBeforeMalformed = addExpectedRequest(
		kValidBeforeMalformedSequence,
		MakePayload(52, 0, 64),
		true,
		"valid-before-malformed");
	auto sentinelRequest = addExpectedRequest(
		kSentinelSequence,
		MakePayload(53, 0, 512),
		true,
		"healthy-sentinel");

	CLGS_AUTHEN_REQT forbiddenAfterMalformed;
	forbiddenAfterMalformed.seq = kForbiddenAfterMalformedSequence;
	forbiddenAfterMalformed.accounttoken = MakePayload(54, 0, 128);
	forbiddenAfterMalformed.reconnect = true;
	forbiddenAfterMalformed.connectSessionKey = "must-not-dispatch";

	const auto managedRoundTripCount =
		static_cast<std::size_t>(reconnectCycles) * payloadSizes.size();
	const auto expectedClientAckCount = managedRoundTripCount + 1;

	auto* network = NetworkServer::GetInstance();
	RoundTripState state;

	JobDispatcher serverDispatcher(
		[&](std::int64_t sessionID, PacketHolder holder)
		{
			{
				std::lock_guard guard(state.lock);
				++state.serverDispatches;
			}
			const auto* request = holder->message.AsCLGS_AUTHEN_REQ();
			if (request == nullptr)
			{
				{
					std::lock_guard guard(state.lock);
					++state.unexpectedServerMessages;
				}
				state.signal.notify_all();
				return;
			}

			bool validRequest = false;
			{
				std::lock_guard guard(state.lock);
				const auto expected = expectedRequests.find(request->seq);
				if (expected == expectedRequests.end())
				{
					++state.unexpectedRequests;
				}
				else
				{
					validRequest = request->accounttoken == expected->second.accountToken
						&& request->reconnect == expected->second.reconnect
						&& request->connectSessionKey == expected->second.sessionKey;
					if (!validRequest)
					{
						++state.payloadMismatches;
					}
					if (!state.serverReceived.insert(request->seq).second)
					{
						++state.duplicateRequests;
					}
				}
			}
			state.signal.notify_all();

			CLGS_AUTHEN_ACKT ack;
			ack.seq = request->seq;
			ack.result = static_cast<std::int32_t>(
				validRequest ? EResultID::R_SUCCESS : EResultID::R_INVALID_DATA_FORMAT);
			if (auto session = network->FindSession(sessionID); session != nullptr)
			{
				session->Send(ack);
			}
		},
		2);

	JobDispatcher clientDispatcher(
		[&](std::int64_t, PacketHolder holder)
		{
			if (const auto* connectAck = holder->message.AsFConnectAck();
				connectAck != nullptr)
			{
				{
					std::lock_guard guard(state.lock);
					++state.clientDispatches;
					++state.connectionAcks;
					if (connectAck->result != EResultID::R_SUCCESS)
					{
						++state.badConnectionAcks;
					}
				}
				state.signal.notify_all();
				return;
			}

			const auto* ack = holder->message.AsCLGS_AUTHEN_ACK();
			if (ack == nullptr)
			{
				{
					std::lock_guard guard(state.lock);
					++state.clientDispatches;
					++state.unexpectedClientMessages;
				}
				state.signal.notify_all();
				return;
			}

			{
				std::lock_guard guard(state.lock);
				++state.clientDispatches;
				if (expectedRequests.find(ack->seq) == expectedRequests.end())
				{
					++state.unexpectedAcks;
				}
				else
				{
					if (ack->result != static_cast<std::int32_t>(EResultID::R_SUCCESS))
					{
						++state.ackMismatches;
					}
					if (!state.clientReceived.insert(ack->seq).second)
					{
						++state.duplicateAcks;
					}
				}
			}
			state.signal.notify_all();
		},
		2);

	BOOST_TEST(network->Initialize(4, 512 * 1024, 2048, 64));
	const auto port = network->Listen(0, &serverDispatcher);
	BOOST_TEST(port != 0);

	if (port != 0)
	{
		// First abandon a frame halfway through, then prove subsequent clean
		// connections and packets are not contaminated by the old receive state.
		BOOST_TEST(SendTruncatedFrame(network, port));
		BOOST_TEST(WaitUntil(
			[&]() { return network->GetSessionCount() == 0; },
			kWaitTimeout));

		std::int64_t previousSessionID = 0;
		for (int cycle = 0; cycle < reconnectCycles; ++cycle)
		{
			network->Connect("127.0.0.1", port, &clientDispatcher);

			std::shared_ptr<NetworkSession> clientSession;
			BOOST_TEST(WaitUntil(
				[&]()
				{
					clientSession = FindConnectedClient(network, port);
					return clientSession != nullptr;
				},
				kWaitTimeout));

			if (clientSession == nullptr)
			{
				break;
			}
			BOOST_TEST(clientSession->GetSessionID() != previousSessionID);
			previousSessionID = clientSession->GetSessionID();
			BOOST_TEST(WaitForConnectionAckCount(
				state,
				static_cast<std::size_t>(cycle + 1),
				kWaitTimeout));

			for (std::size_t packetIndex = 0; packetIndex < payloadSizes.size(); ++packetIndex)
			{
				const auto sequence = 10000 + cycle * 1000
					+ static_cast<std::int32_t>(packetIndex);
				CLGS_AUTHEN_REQT request;
				request.seq = sequence;
				const auto& expected = expectedRequests.at(sequence);
				request.accounttoken = expected.accountToken;
				request.reconnect = expected.reconnect;
				request.connectSessionKey = expected.sessionKey;
				clientSession->Send(request);
			}

			const auto expectedThroughThisCycle =
				static_cast<std::size_t>(cycle + 1) * payloadSizes.size();
			BOOST_TEST(WaitForAckCount(
				state,
				expectedThroughThisCycle,
				kWaitTimeout));

			{
				std::lock_guard guard(state.lock);
				BOOST_TEST_EQ(
					state.serverReceived.size(),
					expectedThroughThisCycle);
			}

			BOOST_TEST(clientSession->Disconnect());
			BOOST_TEST(WaitUntil(
				[&]()
				{
					return network->GetSessionCount() == 0
						&& !clientSession->IsConnected()
						&& clientSession->GetPendingSendBytes() == 0;
				},
				kWaitTimeout));
			clientSession.reset();
		}

		// Exercise real TCP fragmentation (including a 1/1/2-byte header split)
		// and several independently encrypted frames coalesced into one write.
		{
			boost::asio::io_context rawContext;
			boost::asio::ip::tcp::socket rawSocket(rawContext);
			BOOST_TEST(ConnectRawSocket(rawSocket, network, port, 0));

			const auto fragmentedFrame = EncryptForWire(
				SerializeForWire(rawValidRequests.front()));
			BOOST_TEST(WriteFrameInFragments(rawSocket, fragmentedFrame));

			std::vector<char> coalescedFrames;
			for (std::size_t index = 1; index < rawValidRequests.size(); ++index)
			{
				auto frame = EncryptForWire(SerializeForWire(rawValidRequests[index]));
				coalescedFrames.insert(
					coalescedFrames.end(),
					frame.begin(),
					frame.end());
			}

			boost::system::error_code writeError;
			const auto written = boost::asio::write(
				rawSocket,
				boost::asio::buffer(coalescedFrames),
				writeError);
			BOOST_TEST(!writeError);
			BOOST_TEST_EQ(written, coalescedFrames.size());
			BOOST_TEST(WaitForServerCount(
				state,
				managedRoundTripCount + rawValidRequests.size(),
				kWaitTimeout));

			CloseRawSocket(rawSocket);
			BOOST_TEST(WaitUntil(
				[&]() { return network->GetSessionCount() == 0; },
				kWaitTimeout));
		}

		// Keep a healthy managed connection alive while malformed raw peers are
		// rejected, proving a protocol error is isolated to the offending session.
		network->Connect("127.0.0.1", port, &clientDispatcher);
		std::shared_ptr<NetworkSession> sentinelSession;
		BOOST_TEST(WaitUntil(
			[&]()
			{
				sentinelSession = FindConnectedClient(network, port);
				return sentinelSession != nullptr;
			},
			kWaitTimeout));
		BOOST_TEST(sentinelSession != nullptr);
		if (sentinelSession != nullptr)
		{
			BOOST_TEST(sentinelSession->GetSessionID() != previousSessionID);
			BOOST_TEST(WaitForConnectionAckCount(
				state,
				static_cast<std::size_t>(reconnectCycles + 1),
				kWaitTimeout));
			BOOST_TEST_EQ(network->GetSessionCount(), 2U);

			// A valid frame before the malformed frame must be delivered exactly
			// once; a valid frame following it in the same write must never dispatch.
			auto validThenInvalid = EncryptForWire(
				SerializeForWire(validBeforeMalformed));
			const auto zeroLengthFrame = MakeLengthHeader(0);
			validThenInvalid.insert(
				validThenInvalid.end(),
				zeroLengthFrame.begin(),
				zeroLengthFrame.end());
			const auto forbiddenFrame = EncryptForWire(
				SerializeForWire(forbiddenAfterMalformed));
			validThenInvalid.insert(
				validThenInvalid.end(),
				forbiddenFrame.begin(),
				forbiddenFrame.end());
			BOOST_TEST(SendRawFrameExpectDisconnect(
				network,
				port,
				2,
				validThenInvalid));
			BOOST_TEST(WaitForServerCount(
				state,
				managedRoundTripCount + rawValidRequests.size() + 1,
				kWaitTimeout));

			std::size_t dispatchesBeforeRejectedFrames = 0;
			{
				std::lock_guard guard(state.lock);
				BOOST_TEST_EQ(
					state.serverReceived.count(kValidBeforeMalformedSequence),
					1U);
				BOOST_TEST_EQ(
					state.serverReceived.count(kForbiddenAfterMalformedSequence),
					0U);
				dispatchesBeforeRejectedFrames = state.serverDispatches;
			}

			constexpr auto maxBodySize = static_cast<flatbuffers::uoffset_t>(
				FrameDecoder::MAX_FRAME_SIZE - PACKET_HEADER_SIZE);
			const auto bodyLimitPlusOne = MakeLengthHeader(maxBodySize + 1);
			BOOST_TEST(SendRawFrameExpectDisconnect(
				network, port, 2, bodyLimitPlusOne));

			const auto impossibleLength = MakeLengthHeader(
				std::numeric_limits<flatbuffers::uoffset_t>::max());
			BOOST_TEST(SendRawFrameExpectDisconnect(
				network, port, 2, impossibleLength));

			BOOST_TEST(SendRawFrameExpectDisconnect(
				network, port, 2, zeroLengthFrame));

			std::vector<char> invalidBody(64, '\0');
			for (std::size_t index = 0; index < invalidBody.size(); ++index)
			{
				invalidBody[index] = static_cast<char>((index * 19U + 7U) & 0xffU);
			}
			BOOST_TEST(SendRawFrameExpectDisconnect(
				network, port, 2, MakeBodyFrame(invalidBody)));

			auto corruptedRoot = SerializeForWire(forbiddenAfterMalformed);
			flatbuffers::WriteScalar<flatbuffers::uoffset_t>(
				corruptedRoot.data() + PACKET_HEADER_SIZE,
				std::numeric_limits<flatbuffers::uoffset_t>::max());
			corruptedRoot = EncryptForWire(std::move(corruptedRoot));
			BOOST_TEST(SendRawFrameExpectDisconnect(
				network, port, 2, corruptedRoot));

			const auto unencryptedValidFrame = SerializeForWire(forbiddenAfterMalformed);
			BOOST_TEST(SendRawFrameExpectDisconnect(
				network, port, 2, unencryptedValidFrame));

			const auto noneMessage = SerializeMessageTypeForWire(MessageID::NONE);
			BOOST_TEST(SendRawFrameExpectDisconnect(
				network, port, 2, noneMessage));

			const auto unknownMessage = SerializeMessageTypeForWire(
				static_cast<MessageID>(123456789));
			BOOST_TEST(SendRawFrameExpectDisconnect(
				network, port, 2, unknownMessage));

			BOOST_TEST(sentinelSession->IsConnected());
			BOOST_TEST_EQ(network->GetSessionCount(), 2U);
			{
				std::lock_guard guard(state.lock);
				BOOST_TEST_EQ(
					state.serverDispatches,
					dispatchesBeforeRejectedFrames);
			}

			sentinelSession->Send(sentinelRequest);
			BOOST_TEST(WaitForAckCount(
				state,
				expectedClientAckCount,
				kWaitTimeout));
			BOOST_TEST(WaitForServerCount(
				state,
				expectedRequests.size(),
				kWaitTimeout));

			BOOST_TEST(sentinelSession->Disconnect());
			BOOST_TEST(WaitUntil(
				[&]()
				{
					return network->GetSessionCount() == 0
						&& !sentinelSession->IsConnected();
				},
				kWaitTimeout));
		}
	}

	network->Shutdown();

	{
		std::lock_guard guard(state.lock);
		BOOST_TEST_EQ(state.serverReceived.size(), expectedRequests.size());
		BOOST_TEST_EQ(state.clientReceived.size(), expectedClientAckCount);
		BOOST_TEST_EQ(state.serverReceived.count(kTruncatedSequence), 0U);
		BOOST_TEST_EQ(state.serverReceived.count(kForbiddenAfterMalformedSequence), 0U);
		BOOST_TEST_EQ(state.duplicateRequests, 0U);
		BOOST_TEST_EQ(state.duplicateAcks, 0U);
		BOOST_TEST_EQ(state.unexpectedRequests, 0U);
		BOOST_TEST_EQ(state.unexpectedAcks, 0U);
		BOOST_TEST_EQ(state.payloadMismatches, 0U);
		BOOST_TEST_EQ(state.ackMismatches, 0U);
		BOOST_TEST_EQ(state.unexpectedServerMessages, 0U);
		BOOST_TEST_EQ(state.unexpectedClientMessages, 0U);
		BOOST_TEST_EQ(state.serverDispatches, expectedRequests.size());
		BOOST_TEST_EQ(
			state.connectionAcks,
			static_cast<std::size_t>(reconnectCycles + 1));
		BOOST_TEST_EQ(
			state.clientDispatches,
			state.connectionAcks + state.clientReceived.size());
		BOOST_TEST_EQ(state.badConnectionAcks, 0U);
	}

	std::cout << "Dummy network test: " << managedRoundTripCount + 1
		<< " exact request/ACK round trips, " << rawValidRequests.size() + 1
		<< " exact raw requests, " << reconnectCycles + 1
		<< " connect/disconnect cycles, malformed frames rejected, serialized frame sizes "
		<< minimumFrameSize << ".." << maximumFrameSize << " bytes\n";
	return boost::report_errors();
}
