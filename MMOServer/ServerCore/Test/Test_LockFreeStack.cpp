

#define INIT_DATA 0x0000000055555555
#define INIT_COUNT  0
#define DATA_COUNT 1000
#define THREAD_NUM 8

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


HANDLE g_Thread[THREAD_NUM];

LockFreeStack<TestData_LockFreeQ*> g_Stack;

LONG g_Data = 0;
LONG g_TempData = 0;


unsigned int __stdcall TestThread_LockFreeStack(LPVOID param)
{
	TestData_LockFreeQ** dataArray = (TestData_LockFreeQ**)param;

	while (1)
	{
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
		//Sleep(1000);

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
			g_Stack.Push(dataArray[i]);
		}

		//Sleep(2000);

		memset(dataArray, 0, sizeof(TestData_LockFreeQ*) * DATA_COUNT);

		int failCount = 0;

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (!g_Stack.Pop(&dataArray[i]))
			{
				failCount++;
			}
		}
		//Sleep(1000);
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
void Start_LockFreeStack()
{

	TestData_LockFreeQ* dataArray[THREAD_NUM][DATA_COUNT];

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		for (int j = 0; j < DATA_COUNT; ++j)
		{
			dataArray[i][j] = new TestData_LockFreeQ;

			if (dataArray[i][j] == nullptr)
			{
				CRASH();
			}
			if (dataArray[i][j]->_Data != INIT_DATA)
			{
				CRASH();
			}
			if (dataArray[i][j]->_RefCount != INIT_COUNT)
			{
				CRASH();
			}
		}

	}
	for (int i = 0; i < THREAD_NUM; ++i)
	{
		g_Thread[i] = (HANDLE)_beginthreadex(NULL, 0, TestThread_LockFreeStack, (void*)dataArray[i], 0, NULL);
	}

	while (true)
	{

		wprintf(L"Stack Count %d\n", g_Stack.m_Count);
		//wprintf(L"Push Count %d\n", g_Stack.m_PushCount);
		//wprintf(L"Pop Count %d\n", g_Stack.m_PopCount);
		Sleep(500);
	}
}