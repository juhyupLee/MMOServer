

//#include "ServerCore/PlayServer.h"
//#include "ServerCore/Test/Test_LockFreeStack.h"
#include "PlayServer.h"
int main()
{
	PlayServer server;

	server.Initialize();
	server.Start();
	server.Run();

	while(true)
	{
		
	}
	//auto temp = std::allocate_shared<TempPlayer>(MyAllocator<TempPlayer>());
	//StartMemoryPool();
	//Start_LockFreeStack();
	//MemoryPool mem;
	//mem.Alloc(10);
	//MemoryPool temp;
	//temp.Alloc(10);

	//auto value = temp.Alloc(sizeof(TempPlayer));

	/*std::vector<StackVariant> tempVec;

	tempVec.push_back(new LockFreeStack<Buffer<64>>());
*/


	//while(true)
	//{
	//	std::random_device rd;  // 하드웨어 기반 난수 생성기
	//	std::mt19937 gen(rd()); // Mersenne Twister 19937 PRNG
	//	std::uniform_int_distribution<int> dist(0, 1000000); // 1~100 사이의 정수 난수

	//	auto value1 = std::to_string(dist(gen));
	//	auto value2 = std::to_string(dist(gen));

	//	CLGS_AUTHEN_REQT temp;
	//	temp.seq = 4;
	//	temp.accounttoken = value1;
	//	temp.connectSessionKey = value2;
	//	auto buffer = send(temp);

	//	auto meesageHolder = recv(buffer);
	//	auto realMesasge = meesageHolder->message.AsCLGS_AUTHEN_REQ();

	//	assert(realMesasge->accounttoken == value1);
	//	assert(realMesasge->connectSessionKey == value2);
	//}
	//	


	//std::wstring tmep2 = L"dfdf";

	//temp.accounttoken = tmep2;

	//MyMMOServer* mmoServer = new MyMMOServer();

	//TimeOutOption timeOutOption;
	//
	//SocketOption sockOption;
	//timeOutOption._LoginTimeOut = 20000;
	//timeOutOption._HeartBeatTimeOut = 60000;
	//timeOutOption._OptionOn = false;

	//sockOption._KeepAliveOption.onoff = 0;
	//sockOption._Linger = true;
	//sockOption._TCPNoDelay = false;
	//sockOption._SendBufferZero = false;
	//bool bServerStartFlag = false;

	//int threadNum = 8;
	//int maxUser = 1000;
	//int runningThread = 4;
	////
	////wprintf(L"워커스레드 갯수:");
	////wscanf_s(L"%d", &threadNum);

	////wprintf(L"러닝 스레드 갯수:");
	////wscanf_s(L"%d", &runningThread);

	////wprintf(L"최대 유저수:");
	////wscanf_s(L"%d", &maxUser);


	//mmoServer->MMOServerStart(nullptr, 40000, runningThread, sockOption, threadNum, maxUser, timeOutOption);
	//bServerStartFlag = true;


	//while (true)
	//{
	//	if (GetAsyncKeyState(VK_F4))
	//	{
	//		break;
	//	}
	//	if (_kbhit())
	//	{
	//		WCHAR temp = _getwch();

	//		if (temp == L'Q' || temp == L'q')
	//		{
	//			if (bServerStartFlag)
	//			{
	//				bServerStartFlag = false;
	//				mmoServer->MMOServerStop();
	//			}
	//			else
	//			{
	//				wprintf(L"이미 서버가 종료되었습니다\n");
	//			}
	//		}
	//		if (temp == L's' || temp == L'S')
	//		{
	//			if (!bServerStartFlag)
	//			{
	//				bServerStartFlag = true;
	//				mmoServer->MMOServerStart(nullptr, 40000, runningThread, sockOption, threadNum, maxUser, timeOutOption);
	//			}
	//			else
	//			{
	//				wprintf(L"서버가 이미 가동중입니다\n");
	//			}
	//		}
	//	}
	//	mmoServer->ServerMonitorPrint();
	//	Sleep(999);
	//}

	//if (bServerStartFlag)
	//{
	//	mmoServer->ServerStop();
	//}
	//delete mmoServer;

	//return 0;

}