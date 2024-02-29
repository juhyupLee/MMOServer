#include "MonitorLanClient.h"
MonitorLanClient::MonitorLanClient()
	:LanClient(this)
{
}
MonitorLanClient::~MonitorLanClient()
{
}
void MonitorLanClient::OnEnterJoinServer()
{
}

void MonitorLanClient::OnLeaveServer()
{
}

void MonitorLanClient::OnRecv(LanPacket* packet)
{
}

void MonitorLanClient::OnError(int errorcode, WCHAR* errorMessage)
{
}
