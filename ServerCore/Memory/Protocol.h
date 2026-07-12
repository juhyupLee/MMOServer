#pragma once

#include <cstdint>

#define CONST_KEY 0xa9
struct LanHeader
{
	uint16_t _Len;
};

#pragma pack(push,1)
struct NetHeader
{
	std::uint8_t _Code;
	uint16_t _Len;
	std::uint8_t _RandKey;
	std::uint8_t _CheckSum;
};
#pragma pack(pop)
