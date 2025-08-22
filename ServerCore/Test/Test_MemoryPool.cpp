
#define INIT_DATA 0x123456789
#define INIT_COUNT  0
#define DATA_COUNT 1000000
#define THREAD_NUM 4
#define TEST_COUNT 1000000
#define POOL_MODE 2

#define CALL_COUNT 100


struct TestData_MemoryPool
{
	TestData_MemoryPool()
	{
		_MyData = INIT_COUNT;
	}

	~TestData_MemoryPool()
	{
		_MyData = INIT_COUNT;
	}
	char buffer[200];
	std::atomic<int64_t> _MyData;
};

HANDLE g_Thread_MemoryPool[THREAD_NUM];
bool g_ExitMemoryPool = false;

std::recursive_mutex g_lock;
std::recursive_mutex g_lock2;

std::queue<std::shared_ptr<TestData_MemoryPool>> allocQ;
std::queue<std::shared_ptr<TestData_MemoryPool>> allocQ2;

#if POOL_MODE == 2

void TestThread_Shared()
{
	const auto testCount = 1000000;
	int popCount = 0;
	Timer A;
	while (true)
	{
		std::lock_guard guard(g_lock2);
		if (!allocQ2.empty())
		{
			auto ptr = allocQ2.front();
			allocQ2.pop();
			popCount++;

			if (ptr->_MyData.load() != 1)
			{
				CRASH();
			}
		}
		else
		{
			auto ptr = std::make_shared<TestData_MemoryPool>();
			ptr->_MyData++;
			allocQ2.push(ptr);
		}

		if (popCount >= TEST_COUNT)
		{
			A.stop("New");
			popCount = 0;
			A.Start();
		}
	}
}
void TestThread_AllocPush()
{
	while(true)
	{
		std::lock_guard guard(g_lock);
		auto ptr = std::allocate_shared<TestData_MemoryPool>(MyAllocator<TestData_MemoryPool>());
		ptr->_MyData++;
		allocQ.push(ptr);
	}
	
}

void TestThread_Alloc()
{
	int popCount = 0;
	Timer A;

	while (true)
	{
		std::lock_guard guard(g_lock);
		if (!allocQ.empty())
		{
			auto ptr = allocQ.front();
			allocQ.pop();
			popCount++;

			if (ptr->_MyData.load() != 1)
			{
				CRASH();
			}
		}
		else
		{
			auto ptr = std::allocate_shared<TestData_MemoryPool>(MyAllocator<TestData_MemoryPool>());
			ptr->_MyData++;
			allocQ.push(ptr);
		}

		if(popCount >= TEST_COUNT)
		{
			A.stop("JUHYUP");
			popCount = 0;
			A.Start();
		}
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


	for (int i = 0; i < THREAD_NUM ; ++i)
	{
		vectorThread.push_back(std::thread(TestThread_Alloc));
	}

	for (int i = 0; i < THREAD_NUM ; ++i)
	{
		vectorThread.push_back(std::thread(TestThread_Shared));
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