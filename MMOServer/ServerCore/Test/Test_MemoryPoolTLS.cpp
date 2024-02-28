

CrashDump g_CrashDump;

#define INIT_DATA 0x0000000055555555
#define INIT_COUNT  0
#define DATA_COUNT 1000000
#define THREAD_NUM 8

#define POOL_MODE 3

#define CALL_COUNT 100
struct Player
{
	virtual ~Player()
	{
	}
	int mAge;
	int mLeve;
};

struct Monster
{
	//Player()
	////{
	/////*	wprintf(L"hi");*/
	////	mAge = 10;
	////	mLeve = 20;
	////}
	int64_t mAge;
	char name[10];

};
struct TestData
{
	TestData()
	{
		_Data = INIT_DATA;
		_RefCount = INIT_COUNT;
	}
	//virtual ~TestData()
	//{

	//}
	char buffer[1024];
	//Player buffer[62];
	int64_t _Data;
	LONG _RefCount;
	//Monster buffer4[100000];
	//Player buffer2[100000];
	//Player buffer[270];


};


CrashDump memoryDump;

HANDLE g_Thread_MemoryPoolTLS[THREAD_NUM];
bool g_Exit = false;

MemoryPool_TLS<TestData> g_MemoryPool(10000);

void Crash()
{
	int* p = nullptr;
	*p = 10;
}
unsigned int __stdcall TestThread_NewDeleteVSAllocFree(LPVOID param)
{
	int count = 0;
	TestData** dataArray = new TestData * [DATA_COUNT];

	while (count < CALL_COUNT)
	{
		//----------------------------------------------------
		// New Delete
		//----------------------------------------------------
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			dataArray[i] = new TestData;
		}

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			delete dataArray[i];
		}

		//---------------------------------------------------------------
		// Alloc Free
		//---------------------------------------------------------------

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			dataArray[i] = g_MemoryPool.Alloc();

		}
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			g_MemoryPool.Free(dataArray[i]);
		}

		count++;
	}
	return 0;
}

#if POOL_MODE == 0
unsigned int __stdcall TestThread_NewDelete(LPVOID param)
{
	TestData** dataArray = (TestData**)param;
	int count = 0;


	while (count < CALL_COUNT)
	{
		if (g_Exit)
		{
			break;
		}

		//g_Profiler.ProfileBegin(L"NewDelete");
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			InterlockedIncrement64(&dataArray[i]->_Data);
			InterlockedIncrement(&dataArray[i]->_RefCount);
		}
		//Sleep(0);

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i]->_Data != INIT_DATA + 1)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT + 1)
			{
				Crash();
			}
		}
		//Sleep(0);

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			InterlockedDecrement64(&dataArray[i]->_Data);
			InterlockedDecrement(&dataArray[i]->_RefCount);

		}
		//Sleep(0);
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}

		//----------------------------------------------------
		// Free ÄÚµå
		//----------------------------------------------------
		g_Profiler.ProfileBegin(L"Delete");
		for (int i = 0; i < DATA_COUNT; ++i)
		{

			delete dataArray[i];

		}
		g_Profiler.ProfileEnd(L"Delete");
		//Sleep(0);


		//memset(dataArray, 0, sizeof(TestData*) * DATA_COUNT);

		//----------------------------------------------------
		// Alloc ÄÚµå
		//----------------------------------------------------
		g_Profiler.ProfileBegin(L"New");
		for (int i = 0; i < DATA_COUNT; ++i)
		{

			dataArray[i] = new TestData;

			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}
		g_Profiler.ProfileEnd(L"New");

		//Sleep(2);
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i] == nullptr)
			{
				Crash();
			}
			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}

		//Sleep(0);

		//g_Profiler.ProfileEnd(L"NewDelete");

		count++;
	}
	return 0;
}
#endif 

#if POOL_MODE == 1
unsigned int __stdcall TestThread_Pool(LPVOID param)
{
	TestData** dataArray = (TestData**)param;
	int count = 0;
	while (true)
	{
		if (g_Exit)
		{
			break;
		}

		//g_Profiler.ProfileBegin(L"Pool");
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			InterlockedIncrement64(&dataArray[i]->_Data);
			InterlockedIncrement(&dataArray[i]->_RefCount);
		}
		//Sleep(0);

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i]->_Data != INIT_DATA + 1)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT + 1)
			{
				Crash();
			}
		}
		//Sleep(0);

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			InterlockedDecrement64(&dataArray[i]->_Data);
			InterlockedDecrement(&dataArray[i]->_RefCount);

		}
		//Sleep(0);
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}

		//----------------------------------------------------
		// Free ÄÚµå
		//----------------------------------------------------
		//g_Profiler.ProfileBegin(L"PoolFree");
		for (int i = 0; i < DATA_COUNT; ++i)
		{

			g_MemoryPool.Free(dataArray[i]);

		}
		//g_Profiler.ProfileEnd(L"PoolFree");
		//Sleep(0);


		//----------------------------------------------------
		// Alloc ÄÚµå
		//----------------------------------------------------
		//g_Profiler.ProfileBegin(L"PoolAlloc");
		for (int i = 0; i < DATA_COUNT; ++i)
		{

			dataArray[i] = g_MemoryPool.Alloc();


			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}
		//g_Profiler.ProfileEnd(L"PoolAlloc");

		//Sleep(2);
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i] == nullptr)
			{
				Crash();
			}
			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}
		//g_Profiler.ProfileEnd(L"Pool");
		//Sleep(0)
		count++;
	}
	return 0;
}
#endif

void StartMemoryPoolTLS()
{
#if POOL_MODE ==1

	TestData*** dataArray = new TestData * *[THREAD_NUM];
	for (int i = 0; i < THREAD_NUM; ++i)
	{
		dataArray[i] = new TestData * [DATA_COUNT];
	}


	for (int i = 0; i < THREAD_NUM; ++i)
	{
		for (int j = 0; j < DATA_COUNT; ++j)
		{
			//Test
			dataArray[i][j] = g_MemoryPool.Alloc();

			if (dataArray[i][j] == nullptr)
			{
				Crash();
			}
			if (dataArray[i][j]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i][j]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}

	}

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		g_Thread_MemoryPoolTLS[i] = (HANDLE)_beginthreadex(NULL, 0, TestThread_Pool, (void*)dataArray[i], 0, NULL);
	}


	while (true)
	{
		if (GetAsyncKeyState(VK_F4))
		{
			g_Exit = true;
			break;
		}
		wprintf(L"Chunk Count:%d\n", g_MemoryPool.GetChunkCount());
		Sleep(1000);
	}
#endif

#if POOL_MODE ==0
	TestData*** dataArray = new TestData * *[THREAD_NUM];
	for (int i = 0; i < THREAD_NUM; ++i)
	{
		dataArray[i] = new TestData * [DATA_COUNT];
	}

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		for (int j = 0; j < DATA_COUNT; ++j)
		{
			dataArray[i][j] = new TestData;

			if (dataArray[i][j] == nullptr)
			{
				Crash();
			}
			if (dataArray[i][j]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i][j]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}

	}

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		g_Thread_MemoryPoolTLS[i] = (HANDLE)_beginthreadex(NULL, 0, TestThread_NewDelete, (void*)dataArray[i], 0, NULL);
	}
#endif


#if POOL_MODE == 3

	int temp = sizeof(TestData);
	for (int i = 0; i < THREAD_NUM; ++i)
	{
		g_Thread_MemoryPoolTLS[i] = (HANDLE)_beginthreadex(NULL, 0, TestThread_NewDeleteVSAllocFree, NULL, 0, NULL);
	}


#endif

	WaitForMultipleObjects(THREAD_NUM, g_Thread_MemoryPoolTLS, TRUE, INFINITE);

}