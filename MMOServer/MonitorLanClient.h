#pragma once
#include "LanClientLib.h"

class MonitorLanClient : public LanClient
{
public:
	MonitorLanClient();
	~MonitorLanClient();
public:
	virtual void OnEnterJoinServer() ;
	virtual void OnLeaveServer() ;
	virtual void OnRecv(LanPacket* packet) ;
	virtual void OnError(int errorcode, WCHAR* errorMessage) ;

};