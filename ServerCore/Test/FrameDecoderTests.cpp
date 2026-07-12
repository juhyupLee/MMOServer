#include "../Network/FrameDecoder.h"

#include "../../flatbuffers/flatbuffers/base.h"
#include <boost/core/lightweight_test.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
std::vector<char> MakeFrame(const std::vector<char>& body)
{
	std::vector<char> frame(sizeof(flatbuffers::uoffset_t) + body.size());
	flatbuffers::WriteScalar<flatbuffers::uoffset_t>(
		frame.data(),
		static_cast<flatbuffers::uoffset_t>(body.size()));
	std::copy(body.begin(), body.end(), frame.begin() + sizeof(flatbuffers::uoffset_t));
	return frame;
}

std::vector<char> MakeBody(std::size_t size)
{
	std::vector<char> body(size);
	for (std::size_t index = 0; index < body.size(); ++index)
	{
		body[index] = static_cast<char>((index * 31U + size) % 251U);
	}
	return body;
}

bool WriteViaSocketSegments(RingQ& buffer, const char* source, std::size_t size)
{
	const auto segments = buffer.GetWritableSegments();
	if (segments[0].size + segments[1].size < size)
	{
		return false;
	}

	std::size_t copied = 0;
	for (const auto& segment : segments)
	{
		const auto copySize = std::min(segment.size, size - copied);
		if (copySize == 0)
		{
			continue;
		}

		std::memcpy(segment.data, source + copied, copySize);
		copied += copySize;
		if (copied == size)
		{
			break;
		}
	}

	return copied == size && buffer.CommitWrite(copied);
}

bool WriteViaSocketSegments(RingQ& buffer, const std::vector<char>& source)
{
	return WriteViaSocketSegments(buffer, source.data(), source.size());
}

void MoveEmptyRingTo(RingQ& buffer, int position)
{
	std::vector<char> filler(static_cast<std::size_t>(position), 'x');
	std::vector<char> output(filler.size());
	BOOST_TEST_EQ(buffer.Enqueue(filler.data(), position), position);
	BOOST_TEST_EQ(buffer.Dequeue(output.data(), position), position);
	BOOST_TEST_EQ(buffer.GetReadSize(), 0);
	BOOST_TEST_EQ(buffer.GetReadPosition(), position);
	BOOST_TEST_EQ(buffer.GetwritePosition(), position);
}

void TestPartialHeaderAndBody()
{
	RingQ buffer;
	const auto frame = MakeFrame({ 'a', 'b', 'c', 'd', 'e' });
	std::vector<char> decoded;
	std::uint32_t messageSize = 0;

	BOOST_TEST(WriteViaSocketSegments(buffer, frame.data(), 2));
	BOOST_TEST(FrameDecoder::TryDecode(buffer, decoded, messageSize)
		== FrameDecodeResult::NeedMoreData);
	BOOST_TEST_EQ(buffer.GetReadSize(), 2);

	BOOST_TEST(WriteViaSocketSegments(buffer, frame.data() + 2, 4));
	BOOST_TEST(FrameDecoder::TryDecode(buffer, decoded, messageSize)
		== FrameDecodeResult::NeedMoreData);
	BOOST_TEST_EQ(buffer.GetReadSize(), 6);

	BOOST_TEST(WriteViaSocketSegments(buffer, frame.data() + 6, frame.size() - 6));
	BOOST_TEST(FrameDecoder::TryDecode(buffer, decoded, messageSize)
		== FrameDecodeResult::FrameReady);
	BOOST_TEST_EQ(messageSize, 5U);
	BOOST_TEST(decoded == frame);
	BOOST_TEST_EQ(buffer.GetReadSize(), 0);
}

void TestMultipleFramesInOneRead()
{
	RingQ buffer;
	const auto first = MakeFrame({ '1', '2' });
	const auto second = MakeFrame({ 'a', 'b', 'c' });
	std::vector<char> joined = first;
	joined.insert(joined.end(), second.begin(), second.end());
	BOOST_TEST(WriteViaSocketSegments(buffer, joined));

	std::vector<char> decoded;
	std::uint32_t messageSize = 0;
	BOOST_TEST(FrameDecoder::TryDecode(buffer, decoded, messageSize)
		== FrameDecodeResult::FrameReady);
	BOOST_TEST(decoded == first);
	BOOST_TEST(FrameDecoder::TryDecode(buffer, decoded, messageSize)
		== FrameDecodeResult::FrameReady);
	BOOST_TEST(decoded == second);
	BOOST_TEST(FrameDecoder::TryDecode(buffer, decoded, messageSize)
		== FrameDecodeResult::NeedMoreData);
}

void TestHeaderWrap()
{
	RingQ buffer;
	MoveEmptyRingTo(buffer, RingQ::RING_BUFFER_SIZE - 3);

	const auto frame = MakeFrame({ 'w', 'r', 'a', 'p' });
	const auto segments = buffer.GetWritableSegments();
	BOOST_TEST_EQ(segments[0].size, 2U);
	BOOST_TEST(segments[1].size > 0);
	BOOST_TEST(WriteViaSocketSegments(buffer, frame));

	std::vector<char> decoded;
	std::uint32_t messageSize = 0;
	BOOST_TEST(FrameDecoder::TryDecode(buffer, decoded, messageSize)
		== FrameDecodeResult::FrameReady);
	BOOST_TEST(decoded == frame);
}

void TestPayloadWrap()
{
	RingQ buffer;
	MoveEmptyRingTo(buffer, RingQ::RING_BUFFER_SIZE - 7);

	const auto frame = MakeFrame({ 'p', 'a', 'y', 'l', 'o', 'a', 'd' });
	BOOST_TEST(WriteViaSocketSegments(buffer, frame));

	std::vector<char> decoded;
	std::uint32_t messageSize = 0;
	BOOST_TEST(FrameDecoder::TryDecode(buffer, decoded, messageSize)
		== FrameDecodeResult::FrameReady);
	BOOST_TEST(decoded == frame);
}

void TestBodySizeMatrix()
{
	constexpr auto headerSize = sizeof(flatbuffers::uoffset_t);
	constexpr auto maxBodySize = FrameDecoder::MAX_FRAME_SIZE - headerSize;
	constexpr std::size_t bodySizes[] = {
		0, 1, 2, 3, 4, 15, 16, 127, 128, 255, 256, 1023, 4095,
		maxBodySize,
	};

	for (const auto bodySize : bodySizes)
	{
		RingQ buffer;
		const auto body = MakeBody(bodySize);
		const auto expectedFrame = MakeFrame(body);
		BOOST_TEST_EQ(expectedFrame.size(), headerSize + bodySize);
		BOOST_TEST(WriteViaSocketSegments(buffer, expectedFrame));

		std::vector<char> decoded;
		std::uint32_t messageSize = 0;
		BOOST_TEST(FrameDecoder::TryDecode(buffer, decoded, messageSize)
			== FrameDecodeResult::FrameReady);
		BOOST_TEST_EQ(messageSize, static_cast<std::uint32_t>(bodySize));
		BOOST_TEST(decoded == expectedFrame);
		BOOST_TEST_EQ(decoded.size(), expectedFrame.size());
		BOOST_TEST_EQ(buffer.GetReadSize(), 0);
	}
}

void TestBodySizeLimitPlusOneRejected()
{
	constexpr auto headerSize = sizeof(flatbuffers::uoffset_t);
	constexpr auto maxBodySize = FrameDecoder::MAX_FRAME_SIZE - headerSize;
	constexpr auto invalidBodySize = maxBodySize + 1;

	RingQ buffer;
	std::vector<char> header(headerSize);
	flatbuffers::WriteScalar<flatbuffers::uoffset_t>(
		header.data(),
		static_cast<flatbuffers::uoffset_t>(invalidBodySize));
	BOOST_TEST(WriteViaSocketSegments(buffer, header));

	std::vector<char> decoded;
	std::uint32_t messageSize = 0;
	BOOST_TEST(FrameDecoder::TryDecode(buffer, decoded, messageSize)
		== FrameDecodeResult::ProtocolError);
	BOOST_TEST_EQ(messageSize, static_cast<std::uint32_t>(invalidBodySize));
	BOOST_TEST(decoded.empty());
	BOOST_TEST_EQ(buffer.GetReadSize(), static_cast<int>(header.size()));
}
}

int main()
{
	TestPartialHeaderAndBody();
	TestMultipleFramesInOneRead();
	TestHeaderWrap();
	TestPayloadWrap();
	TestBodySizeMatrix();
	TestBodySizeLimitPlusOneRejected();
	return boost::report_errors();
}
