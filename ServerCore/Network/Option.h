#pragma once

struct TimeOutOption
{
	bool _OptionOn;
	uint32_t _LoginTimeOut;
	uint32_t _HeartBeatTimeOut;
};

struct SocketOption
{
	SocketOption()
		: _TCPNoDelay(true)
		, _SendBufferZero(true)
		, _Linger(true)
		, _KeepAlive(false)
		, _KeepAliveIntervalMs(0)
		, _KeepAliveTimeMs(0)
	{
	}
	bool _TCPNoDelay;
	bool _SendBufferZero;
	bool _Linger;
	bool _KeepAlive;
	uint32_t _KeepAliveIntervalMs;
	uint32_t _KeepAliveTimeMs;
};
