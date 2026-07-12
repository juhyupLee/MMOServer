#pragma once

#include "../Memory/RingBuffer.h"

#include <cstdint>
#include <vector>

enum class FrameDecodeResult
{
	NeedMoreData,
	FrameReady,
	ProtocolError,
};

class FrameDecoder
{
public:
	static constexpr std::size_t MAX_FRAME_SIZE = RingQ::RING_BUFFER_SIZE - 1;

	static FrameDecodeResult TryDecode(
		RingQ& receiveBuffer,
		std::vector<char>& frameBuffer,
		std::uint32_t& messageSize);
};
