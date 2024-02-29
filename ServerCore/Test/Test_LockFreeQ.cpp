
#define THREAD_NUM 2
#define INIT_DATA 0x0000000055555555
#define INIT_COUNT  0
#define DATA_COUNT 3
#define TEST_MODE 1

HANDLE g_EnQThread;
HANDLE g_DeQThread;

int g_TestMode = 0;
bool g_ExitFlag = false;

struct TestData_LockFreeQ
{
	TestData_LockFreeQ()
	{
		_Data = INIT_DATA;
		_RefCount = INIT_COUNT;
	}
	int64_t _Data;
	LONG _RefCount;
};

LockFreeQ<TestData_LockFreeQ*> g_Q;




unsigned int __stdcall TestEnQ(LPVOID param)
{
	TestData_LockFreeQ data;

	while (true)
	{

		InterlockedIncrement64(&data._Data);
		InterlockedIncrement(&data._RefCount);


		if (data._Data != INIT_DATA + 1)
		{
			CRASH();
		}
		if (data._RefCount != INIT_COUNT + 1)
		{
			CRASH();
		}

		InterlockedDecrement64(&data._Data);
		InterlockedDecrement(&data._RefCount);

		if (data._Data != INIT_DATA)
		{
			CRASH();
		}
		if (data._RefCount != INIT_COUNT)
		{
			CRASH();
		}

		g_Q.EnQ(&data);
	}

}


unsigned int __stdcall TestDeQ(LPVOID param)
{
	TestData_LockFreeQ* data = nullptr;

	while (true)
	{


		if (!g_Q.DeQ(&data))
		{
			continue;
		}

		if (data->_Data != INIT_DATA)
		{
			CRASH();
		}
		if (data->_RefCount != INIT_COUNT)
		{
			CRASH();
		}

	}

}

unsigned int __stdcall TestThread(LPVOID param)
{
	TestData_LockFreeQ** dataArray = (TestData_LockFreeQ**)param;

	while (1)
	{
		if (g_ExitFlag)
		{
			break;
		}

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			InterlockedIncrement64(&dataArray[i]->_Data);
			InterlockedIncrement(&dataArray[i]->_RefCount);
		}
		//Sleep(5);

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i]->_Data != INIT_DATA + 1)
			{
				CRASH();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT + 1)
			{
				CRASH();
			}
		}
		//Sleep(5);

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			InterlockedDecrement64(&dataArray[i]->_Data);
			InterlockedDecrement(&dataArray[i]->_RefCount);
		}
		//Sleep(5);
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i]->_Data != INIT_DATA)
			{
				CRASH();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				CRASH();
			}
		}

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			//g_Profiler.ProfileBegin(L"EnQ");
			g_Q.EnQ(dataArray[i]);
			//g_Profiler.ProfileEnd(L"EnQ");

		}

		if (g_TestMode == 2)
		{
			Sleep(0);
		}


		memset(dataArray, 0, sizeof(TestData_LockFreeQ*) * DATA_COUNT);

		int failCount = 0;

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			g_Q.DeQ(&dataArray[i]);
		}
		//
		if (g_TestMode == 3)
		{
			Sleep(0);
		}
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i] == nullptr)
			{
				CRASH();
			}
			if (dataArray[i]->_Data != INIT_DATA)
			{
				CRASH();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				CRASH();
			}
		}
	}


	return 0;
}
void StartLockFreeQTest()
{
	setlocale(LC_ALL, "");

	int threadNum = 0;
	int dataCount = 0;
	HANDLE* threadHandle = nullptr;


	wprintf(L"Thread Count:");
	int error = wscanf_s(L"%d", &threadNum);

	wprintf(L"Data Count:");
	error = wscanf_s(L"%d", &dataCount);


	wprintf(L"=================\n");
	wprintf(L"1.Normal\n");
	wprintf(L"2.EnQ\n");
	wprintf(L"3.DeQ\n");
	wprintf(L"=================\n");
	wprintf(L"TestMode:");
	error = wscanf_s(L"%d", &g_TestMode);



#if TEST_MODE ==1
	//TestData_LockFreeQ* dataArray[THREAD_NUM][DATA_COUNT];
	std::map<int64_t, TestData_LockFreeQ*> DataReUseTest;

	TestData_LockFreeQ*** threadData = new TestData_LockFreeQ * *[threadNum];

	for (int i = 0; i < threadNum; ++i)
	{
		threadData[i] = new TestData_LockFreeQ *[dataCount];
	}


	threadHandle = new HANDLE[threadNum];


	for (int i = 0; i < threadNum; ++i)
	{
		for (int j = 0; j < dataCount; ++j)
		{
			threadData[i][j] = new TestData_LockFreeQ;

			DataReUseTest.insert(std::make_pair((int64_t)threadData[i][j], threadData[i][j]));

			if (threadData[i][j] == nullptr)
			{
				CRASH();
			}
			if (threadData[i][j]->_Data != INIT_DATA)
			{
				CRASH();
			}
			if (threadData[i][j]->_RefCount != INIT_COUNT)
			{
				CRASH();
			}
		}

	}
	for (int i = 0; i < threadNum; ++i)
	{
		for (int j = 0; j < dataCount; ++j)
		{

			auto iter = DataReUseTest.find((int64_t)threadData[i][j]);

			if (iter == DataReUseTest.end())
			{
				CRASH();
			}
			if (threadData[i][j] == nullptr)
			{
				CRASH();
			}
			if (threadData[i][j]->_Data != INIT_DATA)
			{
				CRASH();
			}
			if (threadData[i][j]->_RefCount != INIT_COUNT)
			{
				CRASH();
			}
		}

	}

	for (int i = 0; i < threadNum; ++i)
	{
		threadHandle[i] = (HANDLE)_beginthreadex(NULL, 0, TestThread, (void*)threadData[i], 0, NULL);
	}

#endif

#if TEST_MODE==2

	g_EnQThread = (HANDLE)_beginthreadex(NULL, 0, TestEnQ, NULL, 0, NULL);
	g_DeQThread = (HANDLE)_beginthreadex(NULL, 0, TestDeQ, NULL, 0, NULL);
#endif


	int64_t prevID = g_Q.m_RearID;
	int64_t tempDif = 0;

	while (true)
	{


		if (GetAsyncKeyState(VK_F4))
		{
			g_ExitFlag = true;
			break;
		}
		wprintf(L"[Q Count: %d]\n[Memory AllocCount: %d]\n[m_Rear :%p]\n[m_Front :%p]\n[EnQ TPS:%d]\n[DeQ TPS: %d]\n",
			g_Q.m_Count, g_Q.GetMemoryPoolAllocCount(), g_Q.m_RearCheck->_NodePtr, g_Q.m_FrontCheck->_NodePtr, g_Q.m_EnQTPS, g_Q.m_DeQTPS);

		g_Q.m_EnQTPS = 0;
		g_Q.m_DeQTPS = 0;

		Sleep(1000);

	}

	DWORD workerExit = WaitForMultipleObjects(threadNum, threadHandle, TRUE, INFINITE);

	/*//g_Profiler.ProfileDataOutText(L"ProfileReport.txt");*/

}