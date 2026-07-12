#include "FrameDecoder.h"

#include "../../flatbuffers/flatbuffers/flatbuffers.h"

#include <array>

FrameDecodeResult FrameDecoder::TryDecode(
	RingQ& receiveBuffer,
	std::vector<char>& frameBuffer,
	std::uint32_t& messageSize)
{
	constexpr auto headerSize = sizeof(flatbuffers::uoffset_t);
	constexpr auto maxMessageSize = MAX_FRAME_SIZE - headerSize;

	if (receiveBuffer.GetReadSize() < static_cast<int32_t>(headerSize))
	{
		return FrameDecodeResult::NeedMoreData;
	}

	std::array<char, headerSize> header{};
	if (receiveBuffer.Peek(header.data(), static_cast<int>(header.size()))
		!= static_cast<int>(header.size()))
	{
		return FrameDecodeResult::ProtocolError;
	}

	messageSize = flatbuffers::ReadScalar<flatbuffers::uoffset_t>(header.data());
	if (messageSize > maxMessageSize)
	{
		return FrameDecodeResult::ProtocolError;
	}

	const auto frameSize = headerSize + static_cast<std::size_t>(messageSize);
	if (static_cast<std::size_t>(receiveBuffer.GetReadSize()) < frameSize)
	{
		return FrameDecodeResult::NeedMoreData;
	}

	frameBuffer.resize(frameSize);
	if (receiveBuffer.Dequeue(frameBuffer.data(), static_cast<int>(frameSize))
		!= static_cast<int>(frameSize))
	{
		return FrameDecodeResult::ProtocolError;
	}

	return FrameDecodeResult::FrameReady;
}
