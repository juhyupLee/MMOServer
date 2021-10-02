#include "PacketProcess.h"
#include "SerializeBuffer.h"



void MakePacket_en_PACKET_CS_GAME_RES_LOGIN(NetPacket* packet, uint16_t type, BYTE status, int64_t accountNo)
{
	(*packet) << type << status << accountNo;
}

void MakePacket_en_PACKET_CS_GAME_RES_ECHO(NetPacket* packet, uint16_t type, int64_t accountNo, int64_t sendTick)
{
	(*packet) << type << accountNo << sendTick;
}

void MakePacket_en_PACKET_SS_MONITOR_LOGIN(LanPacket* packet, uint16_t type, int32_t serverNo)
{
	(*packet) << type << serverNo;
}

void MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(LanPacket* packet, uint16_t type, BYTE dataType, int32_t dataValue, int timeStamp)
{
	(*packet) << type <<dataType<<dataValue<<timeStamp;

}
