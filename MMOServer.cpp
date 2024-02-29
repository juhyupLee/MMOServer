#include "MMOServer.h"

#include "Log.h"

#include "CommonProtocol2.h"
#include "PacketProcess.h"

#include "CPUUsage.h"
#include "MonitorPDH.h"
#include "MonitorLanClient.h"


//CPUUsage g_CPUUsage;
MonitorPDH g_MonitorPDH;

MyMMOServer::MyMMOServer()
    :
    m_ClientPool(500,true)
{
    wcscpy_s(m_BlackIPList[0], L"185.216.140.27"); // 네덜란드 블랙 IP

    wcscpy_s(m_WhiteIPList[0], L"127.0.0.1"); // 루프팩
    wcscpy_s(m_WhiteIPList[1], L"10.0.1.2"); // Unit 1
    wcscpy_s(m_WhiteIPList[2], L"10.0.2.2"); // unit 1

   m_MaxTCPRetrans =0;

   m_bMonitorServerLogin = false;
   m_bServerOn = false;
}

MyMMOServer::~MyMMOServer()
{
}


void MyMMOServer::SendMonitorData()
{
   // g_CPUUsage.UpdateCPUTime();
   // g_MonitorPDH.ReNewPDH();

   // time_t curTime;
   // time(&curTime);

   // LanPacket* packet = LanPacket::Alloc();
   // //--------------------------------
   // // Server ON/OFF
   // //--------------------------------
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_SERVER_RUN, (int32_t)m_bServerOn, curTime);
   // m_MonitorClient->SendPacket(packet);


   // //--------------------------------
   // // Server Process CPU 
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_SERVER_CPU, (int32_t)g_CPUUsage.ProcessorTotal(), curTime);
   // m_MonitorClient->SendPacket(packet);

   // //--------------------------------
   // // Server Memory Use
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_SERVER_MEM, (int32_t)(g_MonitorPDH.GetPrivateMemory() / 1024) / 1024, curTime);
   // m_MonitorClient->SendPacket(packet);

   // //--------------------------------
   // // Server Session Count
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_SESSION, (int32_t)m_SessionCount, curTime);
   // m_MonitorClient->SendPacket(packet);

   // //--------------------------------
   // // Server Auth Session Count
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_AUTH_PLAYER, (int32_t)m_AuthSessionCount, curTime);
   // m_MonitorClient->SendPacket(packet);

   // //--------------------------------
   // // Server Game Session Count
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_GAME_PLAYER, (int32_t)m_GameSessionCount, curTime);
   // m_MonitorClient->SendPacket(packet);

   // //--------------------------------
   // // Server Accept TPS
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_ACCEPT_TPS, (int32_t)GetAcceptTPS(), curTime);
   // m_MonitorClient->SendPacket(packet);

   // //--------------------------------
   // // Server Recv TPS 
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_PACKET_RECV_TPS, (int32_t)GetRecvTPS(), curTime);
   // m_MonitorClient->SendPacket(packet);

   // //--------------------------------
   // // Server Send TPS
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_PACKET_SEND_TPS, (int32_t)GetSendTPS(), curTime);
   // m_MonitorClient->SendPacket(packet);


   // //--------------------------------
   // // Server Auth FPS
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_AUTH_THREAD_FPS, (int32_t)GetAuthFPS(), curTime);
   // m_MonitorClient->SendPacket(packet);

   // //--------------------------------
   // // Server Game FPS
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_GAME_THREAD_FPS, (int32_t)GetGameFPS(), curTime);
   // m_MonitorClient->SendPacket(packet);


   // //--------------------------------
   // // Server Packet Pool Use
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_GAME_PACKET_POOL, (int32_t)NetPacket::GetUseCount(), curTime);
   // m_MonitorClient->SendPacket(packet);

   // m_MonitorClient->SendPost();



   // //--------------------------------
   // // 하드웨어 CPU Total
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL, (int32_t)g_CPUUsage.ProcessorUser(), curTime);
   // m_MonitorClient->SendPacket(packet);

   // m_MonitorClient->SendPost();


   // //--------------------------------
   // // 논페이지 메모리 
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY, (int32_t)(g_MonitorPDH.GetNPMemory() / 1024)/1024, curTime);
   // m_MonitorClient->SendPacket(packet);

   // m_MonitorClient->SendPost();


   // //--------------------------------
   // // 서버 이용 가능한 메모리
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY, (int32_t)g_MonitorPDH.GetAvailableMemory(), curTime);
   // m_MonitorClient->SendPacket(packet);

   // m_MonitorClient->SendPost();


   // //--------------------------------
   //// 네트워크 송신량.
   ////--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND, (int32_t)GetNetworkTraffic() / 1024, curTime);
   // m_MonitorClient->SendPacket(packet);


   // //--------------------------------
   // // 네트워크 수신량.
   // //--------------------------------
   // packet = LanPacket::Alloc();
   // MakePacket_en_PACKET_SS_MONITOR_DATA_UPDATE(packet, en_PACKET_SS_MONITOR_DATA_UPDATE, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV, (int32_t)GetNetworkRecvTraffic() / 1024, curTime);
   // m_MonitorClient->SendPacket(packet);

   // m_MonitorClient->SendPost();


}
void MyMMOServer::ServerMonitorPrint()
{

    //if (!m_bMonitorServerLogin)
    //{
    //    LanPacket* packet = LanPacket::Alloc();
    //    MakePacket_en_PACKET_SS_MONITOR_LOGIN(packet, en_PACKET_SS_MONITOR_LOGIN, 1);
    //    m_MonitorClient->SendPacket(packet);
    //    m_MonitorClient->SendPost();
    //}

    //SendMonitorData();


    //wprintf(L"==========OS Resource=========\n");
    //wprintf(L"HardWare U:%.1f  K:%.1f  Total:%.1f\n", g_CPUUsage.ProcessorUser(), g_CPUUsage.ProcessorKernel(), g_CPUUsage.ProcessorTotal());
    //wprintf(L"Process U:%.1f  K:%.1f  Total:%.1f\n", g_CPUUsage.ProcessUser(), g_CPUUsage.ProcessKernel(), g_CPUUsage.ProcessTotal());
    //wprintf(L"Private Mem :%lld[K]\n", g_MonitorPDH.GetPrivateMemory() / 1024);
    //wprintf(L"NP Mem :%lld[K]\n", g_MonitorPDH.GetNPMemory() / 1024);
    //wprintf(L"Available Mem :%.1lf[G]\n", g_MonitorPDH.GetAvailableMemory() / 1024);

    //int64_t retransValue = g_MonitorPDH.GetTCPRetrans();
    //if (m_MaxTCPRetrans < retransValue)
    //{
    //    m_MaxTCPRetrans = retransValue;
    //}
    //wprintf(L"TCP Retransmit/sec  :%lld\n", retransValue);
    //wprintf(L"Max TCP Retransmit/sec  :%lld\n", m_MaxTCPRetrans);

    //wprintf(L"==========TPS=========\n");
    //wprintf(L"Accept TPS : %d\nSend TPS:%d\nRecv TPS:%d\n"
    //    , GetAcceptTPS()
    //    , GetSendTPS()
    //    , GetRecvTPS());

    //wprintf(L"==========Memory=========\n");
    //wprintf(L"Packet PoolAlloc:%d \nPacket Pool Count :%d \nPacket Pool Use Count :%d \nLockFreeQ Memory:%d \nAlloc Memory Count:%d\nFree Memory Count:%d\n"
    //    , NetPacket::GetMemoryPoolAllocCount()
    //    , NetPacket::GetPoolCount()
    //    , NetPacket::GetUseCount()
    //    , GetSendQMeomryCount()
    //    , g_AllocMemoryCount
    //    , g_FreeMemoryCount);

    //wprintf(L"==========Network =========\n");
    //wprintf(L"(1) NetworkTraffic(Send) :%d \n(2) TCP TimeOut ReleaseCount :% d \n"
    //    , GetNetworkTraffic()
    //    , g_TCPTimeoutReleaseCnt);

    //wprintf(L"==========FPS ===========\n");
    //wprintf(L"AuthThread FPS :%d\nGameThread FPS :%d\nSendThread FPS :%d\n"
    //    , GetAuthFPS()
    //    , GetGameFPS()
    //    , GetSendFPS());
    //    
    //wprintf(L"==========Count ===========\n");
    //wprintf(L"(1) Accept Count:%lld\t(2) Disconnect Count :%d\n(3) Session Count:%d\n"
    //    , GetAcceptCount()
    //    , g_DisconnectCount
    //    , m_SessionCount);
    //    
    //wprintf(L"==============================\n");



    ////for (DWORD i = 0; i < m_MaxUserCount; ++i)
    ////{
    ////    if (m_SessionArray[i]->_SessionStatus == 1)
    ////    {
    ////        wprintf(L"Session[%d] Complete Q UseSize:%d\n", i, m_SessionArray[i]->_CompleteRecvPacketQ.GetUsedSize());
    ////    }
    ////}

    //if (GetAsyncKeyState(VK_F2))
    //{
    //    for (DWORD i = 0; i < m_MaxUserCount; ++i)
    //    {
    //        wprintf(L"Session[%d] Status:%d  , SendQ UsedSize:%d , bSending:%d, SendFlag:%d \n", i, m_SessionArray[i]->_SessionStatus, m_SessionArray[i]->_SendQ.GetQCount(), m_SessionArray[i]->_bSending, m_SessionArray[i]->_SendFlag);
    //    }
    //}


    //if (GetAsyncKeyState(VK_F3))
    //{
    //    g_Profiler.ProfileDataOutText(L"ProfileResult.txt");
    //}


    //InterlockedExchange(&g_FreeMemoryCount, 0);
    //InterlockedExchange(&g_AllocMemoryCount, 0);
}

bool MyMMOServer::OnConnectionRequest(WCHAR* ip, uint16_t port)
{
    for (int i = 0; i < MAX_WHITE_IP_COUNT; ++i)
    {
        // White IP와 같으면 바로 Return true
        if (!wcscmp(m_WhiteIPList[i], ip))
        {
            return true;
        }
    }
    _LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_SYSTEM, L"외부 IP:%s", ip);
    return false;
}


Client* MyMMOServer::FindClient(uint64_t sessionID)
{
    Client* findClient = nullptr;

    auto iter = m_ClientMap.find(sessionID);
    if (iter == m_ClientMap.end())
    {
        return nullptr;
    }
    findClient = (*iter).second;

    return findClient;
}




bool MyMMOServer::MMOServerStart(WCHAR* ip, uint16_t port, DWORD runningThread, SocketOption& option, DWORD workerThreadCount, DWORD maxUserCount, TimeOutOption& timeOutOption)
{
    //-------------------------------------------------------------------
    // MMOSession을 상속받은 Client를 미리 동적할당하여 그포인터를 라이브러리쪽으로
    // 세팅하도록 넘긴다.
    // 해제의 책임도 저 라이브러리한테있다.
    //-------------------------------------------------------------------
    Client* clientArr = new Client[maxUserCount];
    Client** clientPointerArray = new Client * [maxUserCount];

    for (size_t i = 0; i < maxUserCount; ++i)
    {
        clientPointerArray[i] = &clientArr[i];
    }
    //-------------------------------------------------------------------
    // 워커스레드, Accept Thread  , Monitoring 스레드 가동
    //-------------------------------------------------------------------

    ServerStart(ip,port, runningThread,option,workerThreadCount,maxUserCount, timeOutOption, (MMOSession**)clientPointerArray);

    //-------------------------------------------------------------
    // 	   Monitor 서버와 연결할 클라이언트 생성 및 Connect
    //-------------------------------------------------------------
    m_MonitorClient = new MonitorLanClient();
    SocketOption clientSockOption;

    clientSockOption._KeepAliveOption.onoff = 0;
    clientSockOption._Linger = true;
    clientSockOption._TCPNoDelay = false;
    clientSockOption._SendBufferZero = false;

    if (!m_MonitorClient->Connect(L"127.0.0.1", 7777, 1, 1, clientSockOption))
    {
        CRASH();
    }

    m_bServerOn = true;

    return true;
}

bool MyMMOServer::MMOServerStop()
{
    //-------------------------------------------------------------------
    // Update Thread 중지
    //-------------------------------------------------------------------
    ServerStop();
    m_bServerOn = false;

    return true;
}





void Client::OnClientJoin_Auth(WCHAR* ip, uint16_t port)
{
}

void Client::OnClientLeave_Auth()
{
}

void Client::OnAuthPacket(NetPacket* packet)
{
    PacketProc(packet);
}

void Client::OnClientJoin_Game(WCHAR* ip, uint16_t port)
{
}

void Client::OnClientLeave_Game()
{
}

void Client::OnGamePacket(NetPacket* packet)
{
    PacketProc(packet);
}

void Client::OnError(int errorcode, WCHAR* errorMessage)
{
}

void Client::OnTimeOut()
{
}

void Client::PacketProc(NetPacket* packet)
{
//    WORD type;
//    if ((*packet).GetPayloadSize() < sizeof(WORD))
//    {
//        CRASH();
//    }
//
//    (*packet) >> type;
//
//
//    switch (type)
//    {
//    case en_PACKET_CS_GAME_REQ_LOGIN:
//        PacketProcess_en_PACKET_CS_GAME_REQ_LOGIN(packet);
//        break;
//    case en_PACKET_CS_GAME_REQ_ECHO:
//        PacketProcess_en_PACKET_CS_GAME_REQ_ECHO(packet);
//        break;
//    case en_PACKET_CS_GAME_REQ_HEARTBEAT:
//        CRASH();
//        break;
//    default:
//#if DISCONLOG_USE ==1
//        _LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"메시지마샬링: 존재하지않는 메시지타입 [Session ID:%llu] [Type:%d]", curSession->_ID, type);
//#endif
//        //InterlockedIncrement(&g_FreeMemoryCount);
//        packet->Free(packet);
//        Disconnect();
//        break;
//    }
}

//void Client::PacketProcess_en_PACKET_CS_GAME_REQ_LOGIN(NetPacket* netPacket)
//{
//
//    if (_SessionStatus != eSessionStatus::AUTH)
//    {
//        CRASH();
//    }
//    if (netPacket->GetPayloadSize() != sizeof(int64_t) + (sizeof(char)*64)  +sizeof(int32_t))
//    {
//#if DISCONLOG_USE ==1
//        _LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"SectorMove: 직렬화패킷 규격에 안맞는 메시지 [Session ID:%llu] [PayloadSize:%d]", sessionID, netPacket->GetPayloadSize());
//#endif
//        //InterlockedIncrement(&g_FreeMemoryCount);
//        netPacket->Free(netPacket);
//        Disconnect();
//        return;
//    }
//
//    (*netPacket) >> _AccountNo;
//
//    (*netPacket).GetData(_SessionKey, sizeof(char) * 64);
//
//    (*netPacket) >> _Version;
//
//    if (_Version != 1)
//    {
//        CRASH();
//    }
//
//    //--------------------------------------------------------
//    // 확인용 메시지를 Client에 전송.
//    //--------------------------------------------------------
//    netPacket->Clear();
//    
//   // MakePacket_en_PACKET_CS_GAME_RES_LOGIN(netPacket, en_PACKET_CS_GAME_RES_LOGIN, 1, _AccountNo);
//    SendUnicast(netPacket);
//
//}

//void Client::PacketProcess_en_PACKET_CS_GAME_REQ_ECHO(NetPacket* netPacket)
//{
//       //------------------------------------------------------------
//       // 테스트용 에코 요청
//       //
//       //	{
//       //		WORD		Type
//       //
//       //		INT64		AccountoNo
//       //		LONGLONG	SendTick
//       //	}
//       //
//       //------------------------------------------------------------	
//
//    if (_SessionStatus != eSessionStatus::GAME)
//    {
//        CRASH();
//    }
//    if (netPacket->GetPayloadSize() != sizeof(int64_t) + sizeof(int64_t))
//    {
//#if DISCONLOG_USE ==1
//        _LOG->WriteLog(L"ChattingServer", SysLog::eLogLevel::LOG_LEVEL_ERROR, L"SectorMove: 직렬화패킷 규격에 안맞는 메시지 [Session ID:%llu] [PayloadSize:%d]", sessionID, netPacket->GetPayloadSize());
//#endif
//        //InterlockedIncrement(&g_FreeMemoryCount);
//        netPacket->Free(netPacket);
//        Disconnect();
//        return;
//    }
//    int64_t tempAccountNo = -1;
//    int64_t tempSendTick;
//
//    (*netPacket) >> tempAccountNo >> tempSendTick;
//
//    if (tempAccountNo != _AccountNo)
//    {
//        CRASH();
//    }
//
//    //--------------------------------------------------------
//    // 확인용 메시지를 Client에 전송.
//    //--------------------------------------------------------
//    netPacket->Clear();
//    //MakePacket_en_PACKET_CS_GAME_RES_ECHO(netPacket, en_PACKET_CS_GAME_RES_ECHO, _AccountNo, tempSendTick);
//    SendUnicast(netPacket);
//}
