
#define INIT_DATA 0x123456789
#define INIT_COUNT  0
#define DATA_COUNT 1000000
#define THREAD_NUM 1

#define POOL_MODE 2

#define CALL_COUNT 100

struct TestData_MemoryPool
{
	TestData_MemoryPool()
	{
		//std::cout << "New Thread ID :" << GetCurrentThreadId() << std::endl;
		_Data = INIT_DATA;
		_RefCount = INIT_COUNT;
		_ThreadID = GetCurrentThreadId();

	}

	~TestData_MemoryPool()
	{
		
		_Data = INIT_DATA;
		_RefCount = INIT_COUNT;

		if(_ThreadID != GetCurrentThreadId())
		{
			CRASH();
		}
	}
	//virtual ~TestData_MemoryPool()
	//{

	//}
	char buffer[400];
	//Player buffer[62];
	int64_t _Data;
	LONG _RefCount;
	int32_t _ThreadID;
	//Monster buffer4[100000];
	//Player buffer2[100000];
	//Player buffer[270];


};

HANDLE g_Thread_MemoryPool[THREAD_NUM];
bool g_ExitMemoryPool = false;

#if POOL_MODE == 2
void TestThread_Shared()
{
	const auto testCount = 1000000;
	while (true)
	{
		Timer B;
		for (int i = 0; i < testCount; ++i)
		{
			auto ptr = std::make_shared<TestData_MemoryPool>();
			InterlockedIncrement64(&ptr->_Data);
			InterlockedIncrement(&ptr->_RefCount);

			if (ptr->_Data != INIT_DATA + 1)
			{
				CRASH();
			}
			if (ptr->_RefCount != INIT_COUNT + 1)
			{
				CRASH();
			}

			InterlockedDecrement64(&ptr->_Data);
			InterlockedDecrement(&ptr->_RefCount);
			if (ptr->_Data != INIT_DATA)
			{
				CRASH();
			}
			if (ptr->_RefCount != INIT_COUNT)
			{
				CRASH();
			}
		}

		B.stop("SharedPtr");
	}
}

void TestThread_Alloc()
{
	const auto testCount = 1000000;
	while (true)
	{
		Timer A;
		for (int i = 0; i < testCount; ++i)
		{
			auto ptr = std::allocate_shared<TestData_MemoryPool>(MyAllocator<TestData_MemoryPool>());
			InterlockedIncrement64(&ptr->_Data);
			InterlockedIncrement(&ptr->_RefCount);

			if (ptr->_Data != INIT_DATA + 1)
			{
				CRASH();
			}
			if (ptr->_RefCount != INIT_COUNT + 1)
			{
				CRASH();
			}

			InterlockedDecrement64(&ptr->_Data);
			InterlockedDecrement(&ptr->_RefCount);
			if (ptr->_Data != INIT_DATA)
			{
				CRASH();
			}
			if (ptr->_RefCount != INIT_COUNT)
			{
				CRASH();
			}
		}
		A.stop("AllocShared");
	}
}
#endif 

#if POOL_MODE == 1
void TestThread_Pool()
{
	while (true)
	{
	
		auto ptr = std::allocate_shared<TestData_MemoryPool>(MyAllocator<TestData_MemoryPool>());
		InterlockedIncrement64(&ptr->_Data);
		InterlockedIncrement(&ptr->_RefCount);

		if (ptr->_Data != INIT_DATA + 1)
		{
			CRASH();
		}
		if (ptr->_RefCount != INIT_COUNT + 1)
		{
			CRASH();
		}

		InterlockedDecrement64(&ptr->_Data);
		InterlockedDecrement(&ptr->_RefCount);
		if (ptr->_Data != INIT_DATA)
		{
			CRASH();
		}
		if (ptr->_RefCount != INIT_COUNT)
		{
			CRASH();
		}
	}
}
#endif

void StartMemoryPool()
{
#if POOL_MODE == 1


	std::vector<std::thread> vectorThread;
	for (int i = 0; i < THREAD_NUM; ++i)
	{
		vectorThread.push_back(std::thread(TestThread_Pool));
	}

#endif
#if POOL_MODE ==2
	std::vector<std::thread> vectorThread;
	for (int i = 0; i < THREAD_NUM; ++i)
	{
		vectorThread.push_back(std::thread(TestThread_Shared));
	}

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		vectorThread.push_back(std::thread(TestThread_Alloc));
	}

#endif

	while (true)
	{
		if (GetAsyncKeyState(VK_F4))
		{
			g_ExitMemoryPool = true;
			break;
		}
		Sleep(1000);
	}



#if POOL_MODE == 3

	int temp = sizeof(TestData_MemoryPool);
	for (int i = 0; i < THREAD_NUM; ++i)
	{
		g_Thread_MemoryPool[i] = (HANDLE)_beginthreadex(NULL, 0, TestThread_NewDeleteVSAllocFree, NULL, 0, NULL);
	}


#endif

	WaitForMultipleObjects(THREAD_NUM, g_Thread_MemoryPool, TRUE, INFINITE);

}