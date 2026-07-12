#include "../ServerCore/Memory/Global.h"
#include "../ServerCore/Network/FrameDecoder.h"

#include <boost/asio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
using Tcp = boost::asio::ip::tcp;

struct ProbeConfig
{
	std::string host{ "127.0.0.1" };
	std::int32_t port{ 7777 };
	std::int32_t timeoutMs{ 3000 };
	std::int32_t frameTimeoutWaitMs{ 12000 };
	bool skipSlowloris{ false };
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

void ConvertBody(char* buffer, std::size_t size)
{
	static constexpr char keyText[] = "MMORPG - MOBIRIX";
	constexpr auto keySize = sizeof(keyText) - 1;
	std::uint8_t key = 0;
	for (std::size_t index = 0; index < size; ++index)
	{
		key = static_cast<std::uint8_t>(
			(key + static_cast<std::uint8_t>(keyText[index % keySize])) * 253 + 195);
		buffer[index] = static_cast<char>(
			static_cast<std::uint8_t>(buffer[index]) ^ key);
	}
}

std::vector<char> SerializeRequest(
	std::int32_t sequence,
	std::size_t payloadSize,
	bool validChecksum = true)
{
	CLGS_AUTHEN_REQT request;
	request.seq = sequence;
	request.accounttoken.resize(payloadSize);
	for (std::size_t index = 0; index < payloadSize; ++index)
	{
		request.accounttoken[index] = static_cast<char>('a' + (index + sequence) % 26);
	}
	request.connectSessionKey = validChecksum
		? "loadtest-fnv1a64:" + Hex64(Fnv1a64(request.accounttoken))
		: "loadtest-fnv1a64:0000000000000000";

	auto holder = CreateMessageHolder(request);
	flatbuffers::FlatBufferBuilder builder;
	const auto offset = MessageHolder::Pack(builder, holder.get());
	builder.FinishSizePrefixed(offset);
	auto detached = builder.Release();
	std::vector<char> frame(
		reinterpret_cast<const char*>(detached.data()),
		reinterpret_cast<const char*>(detached.data()) + detached.size());
	if (frame.size() <= PACKET_HEADER_SIZE
		|| frame.size() > FrameDecoder::MAX_FRAME_SIZE)
	{
		throw std::runtime_error("Serialized frame is outside the protocol limits");
	}
	ConvertBody(
		&frame.at(PACKET_HEADER_SIZE),
		frame.size() - PACKET_HEADER_SIZE);
	return frame;
}

std::vector<char> MakeLengthHeader(flatbuffers::uoffset_t bodySize)
{
	std::vector<char> header(PACKET_HEADER_SIZE);
	flatbuffers::WriteScalar<flatbuffers::uoffset_t>(header.data(), bodySize);
	return header;
}

std::vector<char> MakeInvalidBodyFrame(std::size_t bodySize)
{
	auto frame = MakeLengthHeader(static_cast<flatbuffers::uoffset_t>(bodySize));
	const auto oldSize = frame.size();
	frame.resize(oldSize + bodySize);
	for (std::size_t index = 0; index < bodySize; ++index)
	{
		frame[oldSize + index] = static_cast<char>((index * 37U + 11U) & 0xffU);
	}
	if (bodySize > 0)
	{
		ConvertBody(&frame.at(PACKET_HEADER_SIZE), bodySize);
	}
	return frame;
}

bool ReadExact(Tcp::socket& socket, char* output, std::size_t size, Clock::time_point deadline)
{
	boost::system::error_code modeError;
	socket.non_blocking(true, modeError);
	if (modeError)
	{
		return false;
	}

	std::size_t received = 0;
	while (received < size && Clock::now() < deadline)
	{
		boost::system::error_code error;
		const auto count = socket.read_some(
			boost::asio::buffer(output + received, size - received), error);
		if (!error)
		{
			received += count;
			continue;
		}
		if (error == boost::asio::error::would_block
			|| error == boost::asio::error::try_again)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		return false;
	}
	return received == size;
}

std::unique_ptr<MessageHolderT> ReadMessage(Tcp::socket& socket, std::int32_t timeoutMs)
{
	const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
	std::array<char, PACKET_HEADER_SIZE> header{};
	if (!ReadExact(socket, header.data(), header.size(), deadline))
	{
		return nullptr;
	}
	const auto bodySize = flatbuffers::ReadScalar<flatbuffers::uoffset_t>(header.data());
	if (bodySize > FrameDecoder::MAX_FRAME_SIZE - PACKET_HEADER_SIZE)
	{
		return nullptr;
	}
	std::vector<char> frame(PACKET_HEADER_SIZE + bodySize);
	std::copy(header.begin(), header.end(), frame.begin());
	if (bodySize > 0
		&& !ReadExact(socket, frame.data() + PACKET_HEADER_SIZE, bodySize, deadline))
	{
		return nullptr;
	}
	if (bodySize > 0)
	{
		ConvertBody(&frame.at(PACKET_HEADER_SIZE), bodySize);
	}
	flatbuffers::Verifier verifier(
		reinterpret_cast<const std::uint8_t*>(frame.data()), frame.size());
	if (!VerifySizePrefixedMessageHolderBuffer(verifier))
	{
		return nullptr;
	}
	return std::unique_ptr<MessageHolderT>(
		GetSizePrefixedMessageHolder(frame.data())->UnPack());
}

bool WriteAll(Tcp::socket& socket, const std::vector<char>& bytes)
{
	boost::system::error_code modeError;
	socket.non_blocking(false, modeError);
	if (modeError)
	{
		return false;
	}
	boost::system::error_code error;
	const auto written = boost::asio::write(socket, boost::asio::buffer(bytes), error);
	return !error && written == bytes.size();
}

bool WriteFragments(
	Tcp::socket& socket,
	const std::vector<char>& frame,
	const std::vector<std::size_t>& fragmentSizes)
{
	std::size_t offset = 0;
	for (const auto requested : fragmentSizes)
	{
		if (offset >= frame.size())
		{
			break;
		}
		const auto size = std::min(requested, frame.size() - offset);
		const std::vector<char> part(frame.begin() + offset, frame.begin() + offset + size);
		if (!WriteAll(socket, part))
		{
			return false;
		}
		offset += size;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (offset < frame.size())
	{
		const std::vector<char> tail(frame.begin() + offset, frame.end());
		return WriteAll(socket, tail);
	}
	return true;
}

std::unique_ptr<Tcp::socket> ConnectAndConsumeAck(
	boost::asio::io_context& context,
	const ProbeConfig& config)
{
	Tcp::resolver resolver(context);
	boost::system::error_code error;
	const auto endpoints = resolver.resolve(
		config.host, std::to_string(config.port), error);
	if (error)
	{
		return nullptr;
	}
	auto socket = std::make_unique<Tcp::socket>(context);
	boost::asio::connect(*socket, endpoints, error);
	if (error)
	{
		return nullptr;
	}
	const auto ack = ReadMessage(*socket, config.timeoutMs);
	if (ack == nullptr)
	{
		return nullptr;
	}
	const auto* connectAck = ack->message.AsFConnectAck();
	if (connectAck == nullptr || connectAck->result != EResultID::R_SUCCESS)
	{
		return nullptr;
	}
	return socket;
}

bool ExpectAuthAck(
	Tcp::socket& socket,
	std::int32_t sequence,
	std::int32_t timeoutMs,
	EResultID expectedResult = EResultID::R_SUCCESS)
{
	const auto message = ReadMessage(socket, timeoutMs);
	if (message == nullptr)
	{
		return false;
	}
	const auto* ack = message->message.AsCLGS_AUTHEN_ACK();
	return ack != nullptr && ack->seq == sequence
		&& ack->result == static_cast<std::int32_t>(expectedResult);
}

bool ExpectDisconnect(Tcp::socket& socket, std::int32_t timeoutMs)
{
	boost::system::error_code modeError;
	socket.non_blocking(true, modeError);
	if (modeError)
	{
		return false;
	}
	const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
	std::array<char, 256> buffer{};
	while (Clock::now() < deadline)
	{
		boost::system::error_code error;
		const auto count = socket.read_some(boost::asio::buffer(buffer), error);
		if (error == boost::asio::error::eof
			|| error == boost::asio::error::connection_reset
			|| error == boost::asio::error::operation_aborted
			|| error == boost::asio::error::not_connected)
		{
			return true;
		}
		if (!error && count == 0)
		{
			return true;
		}
		if (error && error != boost::asio::error::would_block
			&& error != boost::asio::error::try_again)
		{
			return true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return false;
}

void CloseSocket(Tcp::socket& socket)
{
	boost::system::error_code ignored;
	socket.close(ignored);
}

bool TestFragmented(const ProbeConfig& config)
{
	boost::asio::io_context context;
	auto socket = ConnectAndConsumeAck(context, config);
	if (socket == nullptr)
	{
		return false;
	}
	constexpr std::int32_t sequence = 710001;
	const auto frame = SerializeRequest(sequence, 6400);
	const bool result = WriteFragments(
		*socket, frame, { 1, 1, 2, 3, 7, 31, 127, 511 })
		&& ExpectAuthAck(*socket, sequence, config.timeoutMs);
	CloseSocket(*socket);
	return result;
}

bool TestCoalesced(const ProbeConfig& config)
{
	boost::asio::io_context context;
	auto socket = ConnectAndConsumeAck(context, config);
	if (socket == nullptr)
	{
		return false;
	}
	const std::array<std::int32_t, 4> sequences{ 720001, 720002, 720003, 720004 };
	const std::array<std::size_t, 4> sizes{ 1, 32, 1024, 6400 };
	std::vector<char> joined;
	for (std::size_t index = 0; index < sequences.size(); ++index)
	{
		auto frame = SerializeRequest(sequences[index], sizes[index]);
		joined.insert(joined.end(), frame.begin(), frame.end());
	}
	if (!WriteAll(*socket, joined))
	{
		return false;
	}
	for (const auto sequence : sequences)
	{
		if (!ExpectAuthAck(*socket, sequence, config.timeoutMs))
		{
			return false;
		}
	}
	CloseSocket(*socket);
	return true;
}

bool TestRejectedFrame(
	const ProbeConfig& config,
	std::vector<char> frame,
	std::int32_t waitMs)
{
	boost::asio::io_context context;
	auto socket = ConnectAndConsumeAck(context, config);
	if (socket == nullptr || !WriteAll(*socket, frame))
	{
		return false;
	}
	const bool disconnected = ExpectDisconnect(*socket, waitMs);
	CloseSocket(*socket);
	return disconnected;
}

bool TestUnderDeclared(const ProbeConfig& config)
{
	auto frame = SerializeRequest(730001, 128);
	const auto bodySize = flatbuffers::ReadScalar<flatbuffers::uoffset_t>(frame.data());
	// Cutting a single trailing alignment byte can still leave a valid
	// FlatBuffer. Cut through the actual object so this is unambiguously an
	// under-declared/truncated frame, with the remaining bytes treated as junk.
	flatbuffers::WriteScalar<flatbuffers::uoffset_t>(frame.data(), bodySize / 2);
	return TestRejectedFrame(config, std::move(frame), config.timeoutMs);
}

bool TestUnencryptedFrame(const ProbeConfig& config)
{
	auto frame = SerializeRequest(740001, 128);
	ConvertBody(&frame.at(PACKET_HEADER_SIZE), frame.size() - PACKET_HEADER_SIZE);
	return TestRejectedFrame(config, std::move(frame), config.timeoutMs);
}

bool TestCorruptedRoot(const ProbeConfig& config)
{
	auto frame = SerializeRequest(750001, 128);
	const auto bodySize = frame.size() - PACKET_HEADER_SIZE;
	ConvertBody(&frame.at(PACKET_HEADER_SIZE), bodySize);
	flatbuffers::WriteScalar<flatbuffers::uoffset_t>(
		&frame.at(PACKET_HEADER_SIZE),
		std::numeric_limits<flatbuffers::uoffset_t>::max());
	ConvertBody(&frame.at(PACKET_HEADER_SIZE), bodySize);
	return TestRejectedFrame(config, std::move(frame), config.timeoutMs);
}

bool TestBadChecksum(const ProbeConfig& config)
{
	boost::asio::io_context context;
	auto socket = ConnectAndConsumeAck(context, config);
	if (socket == nullptr)
	{
		return false;
	}
	constexpr std::int32_t sequence = 760001;
	const auto frame = SerializeRequest(sequence, 512, false);
	const bool result = WriteAll(*socket, frame)
		&& ExpectAuthAck(
			*socket,
			sequence,
			config.timeoutMs,
			EResultID::R_INVALID_DATA_FORMAT);
	CloseSocket(*socket);
	return result;
}

bool TestSlowlorisDeadline(const ProbeConfig& config)
{
	boost::asio::io_context context;
	auto socket = ConnectAndConsumeAck(context, config);
	if (socket == nullptr)
	{
		return false;
	}
	const auto header = MakeLengthHeader(512);
	const std::vector<char> partial(header.begin(), header.begin() + 2);
	if (!WriteAll(*socket, partial))
	{
		return false;
	}
	const bool disconnected = ExpectDisconnect(*socket, config.frameTimeoutWaitMs);
	CloseSocket(*socket);
	return disconnected;
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

ProbeConfig ParseConfig(int argc, char* argv[])
{
	ProbeConfig config;
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
		else if (option == "--timeout-ms") config.timeoutMs = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--frame-timeout-wait-ms") config.frameTimeoutWaitMs = static_cast<std::int32_t>(ParseInteger(nextValue(), option));
		else if (option == "--skip-slowloris") config.skipSlowloris = true;
		else if (option == "--help")
		{
			std::cout
				<< "ProtocolProbe options:\n"
				<< "  --host HOST --port N --timeout-ms N\n"
				<< "  --frame-timeout-wait-ms N --skip-slowloris\n";
			std::exit(0);
		}
		else throw std::runtime_error("Unknown option: " + option);
	}
	if (config.host.empty() || config.port <= 0 || config.port > 65535
		|| config.timeoutMs <= 0 || config.frameTimeoutWaitMs <= 10000)
	{
		throw std::runtime_error("ProtocolProbe configuration is out of range");
	}
	return config;
}
}

int main(int argc, char* argv[])
{
	try
	{
		const auto config = ParseConfig(argc, argv);
		std::size_t passed = 0;
		std::size_t failed = 0;
		auto run = [&](const char* name, auto&& test)
		{
			const bool result = test();
			std::cout << (result ? "[PASS] " : "[FAIL] ") << name << '\n';
			(result ? passed : failed)++;
		};

		run("fragmented header/body + near-max valid payload", [&]()
		{
			return TestFragmented(config);
		});
		run("four coalesced valid frames + exact ACKs", [&]()
		{
			return TestCoalesced(config);
		});
		run("zero-length frame rejected", [&]()
		{
			return TestRejectedFrame(config, MakeLengthHeader(0), config.timeoutMs);
		});
		run("max+1 declared body rejected", [&]()
		{
			constexpr auto maxBody = static_cast<flatbuffers::uoffset_t>(
				FrameDecoder::MAX_FRAME_SIZE - PACKET_HEADER_SIZE);
			return TestRejectedFrame(
				config, MakeLengthHeader(maxBody + 1), config.timeoutMs);
		});
		run("UINT32_MAX declared body rejected", [&]()
		{
			return TestRejectedFrame(
				config,
				MakeLengthHeader(std::numeric_limits<flatbuffers::uoffset_t>::max()),
				config.timeoutMs);
		});
		run("random invalid FlatBuffer rejected", [&]()
		{
			return TestRejectedFrame(config, MakeInvalidBodyFrame(64), config.timeoutMs);
		});
		run("under-declared valid frame rejected", [&]()
		{
			return TestUnderDeclared(config);
		});
		run("unencrypted otherwise-valid frame rejected", [&]()
		{
			return TestUnencryptedFrame(config);
		});
		run("corrupted FlatBuffer root rejected", [&]()
		{
			return TestCorruptedRoot(config);
		});
		run("payload checksum mismatch returns invalid ACK", [&]()
		{
			return TestBadChecksum(config);
		});
		if (!config.skipSlowloris)
		{
			run("partial header held open hits frame deadline", [&]()
			{
				return TestSlowlorisDeadline(config);
			});
		}

		std::cout << "ProtocolProbe summary: pass=" << passed
			<< " fail=" << failed << '\n';
		return failed == 0 ? 0 : 1;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "ProtocolProbe fatal error: " << exception.what() << '\n';
		return 2;
	}
}
