#pragma once
#define CONST_KEY 0xa9
struct LanHeader
{
	uint16_t _Len;
};

#pragma pack(push,1)
struct NetHeader
{
	uint8_t _Code;
	uint16_t _Len;
	uint8_t _RandKey;
	uint8_t _CheckSum;
};
#pragma pack(pop)
