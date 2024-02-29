#pragma once
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"Winmm.lib")
#include <WinSock2.h>
#include <mstcpip.h>
#include <WS2tcpip.h>

struct TimeOutOption
{
	bool _OptionOn;
	DWORD _LoginTimeOut;
	DWORD _HeartBeatTimeOut;
};
struct SocketOption
{
	SocketOption()
		:_TCPNoDelay(true),
		_SendBufferZero(true),
		_Linger(true)
	{
		ZeroMemory(&_KeepAliveOption, sizeof(tcp_keepalive));
	}
	bool _TCPNoDelay;
	bool _SendBufferZero;
	bool _Linger;
	tcp_keepalive _KeepAliveOption;
};

