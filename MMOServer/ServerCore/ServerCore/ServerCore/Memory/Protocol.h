#pragma once
#define CONST_KEY 0xa9
struct LanHeader
{
	uint16_t _Len;
};

#pragma pack(push,1)
struct NetHeader
{
	BYTE _Code;
	uint16_t _Len;
	BYTE _RandKey;
	BYTE _CheckSum;
};
#pragma pack(pop)

