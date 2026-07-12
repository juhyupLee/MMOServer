#pragma once

#include <cstdint>

struct TimeOutOption
{
	bool _OptionOn;
	std::uint32_t _LoginTimeOut;
	std::uint32_t _HeartBeatTimeOut;
};
struct SocketOption
{
	SocketOption()
		: _TCPNoDelay(true)
		, _Linger(false)
		, _KeepAlive(true)
	{
	}
	bool _TCPNoDelay;
	bool _Linger;
	bool _KeepAlive;
};
