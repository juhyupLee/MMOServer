//
//#define INIT_DATA 0x123456789
//#define INIT_COUNT  0
//#define DATA_COUNT 1000000
//#define THREAD_NUM 4
//#define TEST_COUNT 1000000
//#define POOL_MODE 2
//
//#define CALL_COUNT 100
//
//
//struct TestData_MemoryPool
//{
//	TestData_MemoryPool()
//	{
//		_MyData = INIT_COUNT;
//	}
//
//	~TestData_MemoryPool()
//	{
//		_MyData = INIT_COUNT;
//	}
//	char buffer[200];
//	std::atomic<int64_t> _MyData;
//};
//
//HANDLE g_Thread_MemoryPool[THREAD_NUM];
//bool g_ExitMemoryPool = false;
//
//std::recursive_mutex g_lock;
//std::recursive_mutex g_lock2;
//
//std::queue<std::shared_ptr<TestData_MemoryPool>> allocQ;
//std::queue<std::shared_ptr<TestData_MemoryPool>> allocQ2;
//
//#if POOL_MODE == 2
//
//void TestThread_Shared()
//{
//	const auto testCount = 1000000;
//	int popCount = 0;
//	Timer A;
//	while (true)
//	{
//		std::lock_guard guard(g_lock2);
//		if (!allocQ2.empty())
//		{
//			auto ptr = allocQ2.front();
//			allocQ2.pop();
//			popCount++;
//
//			if (ptr->_MyData.load() != 1)
//			{
//				CRASH();
//			}
//		}
//		else
//		{
//			auto ptr = std::make_shared<TestData_MemoryPool>();
//			ptr->_MyData++;
//			allocQ2.push(ptr);
//		}
//
//		if (popCount >= TEST_COUNT)
//		{
//			A.stop("New");
//			popCount = 0;
//			A.Start();
//		}
//	}
//}
//void TestThread_AllocPush()
//{
//	while(true)
//	{
//		std::lock_guard guard(g_lock);
//		auto ptr = std::allocate_shared<TestData_MemoryPool>(MyAllocator<TestData_MemoryPool>());
//		ptr->_MyData++;
//		allocQ.push(ptr);
//	}
//	
//}
//
//void TestThread_Alloc()
//{
//	int popCount = 0;
//	Timer A;
//	bool isPop = false;
//
//	while (true)
//	{
//		{
//			std::lock_guard guard(g_lock);
//			if (!allocQ.empty())
//			{
//				auto ptr = allocQ.front();
//				allocQ.pop();
//				popCount++;
//
//				isPop = true;
//				if (ptr->_MyData.load() != 1)
//				{
//					CRASH();
//				}
//
//				if (isPop)
//				{
//					/*for (volatile int i = 0; i < 50000; ++i)
//					{
//
//					}*/
//				}
//			}
//			else
//			{
//				auto ptr = std::allocate_shared<TestData_MemoryPool>(MyAllocator<TestData_MemoryPool>());
//				ptr->_MyData++;
//				allocQ.push(ptr);
//				isPop = false;
//			}
//		}
//
//		if(popCount >= TEST_COUNT)
//		{
//			A.stop("JUHYUP");
//			popCount = 0;
//			A.Start();
//		}
//	}
//}
//#endif 
//
//#if POOL_MODE == 1
//void TestThread_Pool()
//{
//	while (true)
//	{
//	
//		auto ptr = std::allocate_shared<TestData_MemoryPool>(MyAllocator<TestData_MemoryPool>());
//		InterlockedIncrement64(&ptr->_Data);
//		InterlockedIncrement(&ptr->_RefCount);
//
//		if (ptr->_Data != INIT_DATA + 1)
//		{
//			CRASH();
//		}
//		if (ptr->_RefCount != INIT_COUNT + 1)
//		{
//			CRASH();
//		}
//
//		InterlockedDecrement64(&ptr->_Data);
//		InterlockedDecrement(&ptr->_RefCount);
//		if (ptr->_Data != INIT_DATA)
//		{
//			CRASH();
//		}
//		if (ptr->_RefCount != INIT_COUNT)
//		{
//			CRASH();
//		}
//	}
//}
//#endif
//
//void StartMemoryPool()
//{
//#if POOL_MODE == 1
//
//
//	std::vector<std::thread> vectorThread;
//	for (int i = 0; i < THREAD_NUM; ++i)
//	{
//		vectorThread.push_back(std::thread(TestThread_Pool));
//	}
//
//#endif
//#if POOL_MODE ==2
//	std::vector<std::thread> vectorThread;
//
//
//	for (int i = 0; i < THREAD_NUM ; ++i)
//	{
//		vectorThread.push_back(std::thread(TestThread_Alloc));
//	}
//
//	for (int i = 0; i < THREAD_NUM ; ++i)
//	{
//		vectorThread.push_back(std::thread(TestThread_Shared));
//	}
//
//	
//
//#endif
//
//	while (true)
//	{
//		if (GetAsyncKeyState(VK_F4))
//		{
//			g_ExitMemoryPool = true;
//			break;
//		}
//		Sleep(1000);
//	}
//
//
//
//#if POOL_MODE == 3
//
//	int temp = sizeof(TestData_MemoryPool);
//	for (int i = 0; i < THREAD_NUM; ++i)
//	{
//		g_Thread_MemoryPool[i] = (HANDLE)_beginthreadex(NULL, 0, TestThread_NewDeleteVSAllocFree, NULL, 0, NULL);
//	}
//
//
//#endif
//
//	WaitForMultipleObjects(THREAD_NUM, g_Thread_MemoryPool, TRUE, INFINITE);
//
//}

 //Visual Studio 2022 / Release / x64 권장

#include <Windows.h>
#include <Psapi.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// ======== (선택) 본인 메모리 풀 사용 설정 ========
// 1) 아래 define 주석 해제
// 2) #include "MyAllocator.h" 경로 맞추기
// 3) MyAllocator<T> 가 std::allocator 인터페이스를 만족해야 함
//#define USE_MY_ALLOC
#ifdef USE_MY_ALLOC
#include "MyAllocator.h"
#endif

// ======== 공통 설정 ========
static constexpr int    THREAD_ALLOCERS = 4;        // Cross-thread alloc 스레드 수
static constexpr int    THREAD_FREERS = 4;        // Cross-thread free 스레드 수
static constexpr int    SAME_THREAD_WORKERS = 8;        // Same-thread 워커 수
static constexpr int    OPS_PER_ALLOC_THREAD = 250000*4;  // alloc 스레드당 작업 수
static constexpr int    OPS_PER_SAME_WORKER = 250000*4;  // same-thread 워커당 작업 수
static constexpr int    SAME_THREAD_BATCH = 64;       // same-thread에서 모아두었다가 해제할 배치 크기
static constexpr int    OBJECT_PAD = 200;      // 테스트 오브젝트 내부 버퍼 크기
static constexpr int    VERIFY_SAMPLE_RATE = 1024;     // 검증 비용 줄이기 위해 N회마다 1회 검증

// delayed free (ns) 후보들
static constexpr long long DELAY_NS_NONE = 0;
static constexpr long long DELAY_NS_SMALL = 5'000;     // 5 us
static constexpr long long DELAY_NS_MEDIUM = 50'000;    // 50 us

// ======== 측정 대상 타입 ========
struct TestData_MemoryPool {
    TestData_MemoryPool() : _MyData(0) {}
    ~TestData_MemoryPool() { _MyData = 0; }
    char                     buffer[OBJECT_PAD];
    std::atomic<int64_t>     _MyData;
};

// ======== Allocator alias ========
#ifdef USE_MY_ALLOC
template<class T>
using PoolAlloc = MyAllocator<T>;
#else
#include <memory>
template<class T>
using PoolAlloc = std::allocator<T>;
#endif

// ======== 유틸: 메모리 사용량 ========
static size_t GetMemoryUsageBytes() {
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}

// ======== 유틸: 타이머 ========
class BenchTimer {
public:
    void start() { _begin = clock::now(); }
    void stop() { _end = clock::now(); }
    double ms() const {
        return std::chrono::duration<double, std::milli>(_end - _begin).count();
    }
    double sec() const { return ms() / 1000.0; }
private:
    using clock = std::chrono::high_resolution_clock;
    clock::time_point _begin{}, _end{};
};

// ======== 유틸: 간단 CountDownLatch ========
class CountDownLatch {
public:
    explicit CountDownLatch(int count) : count_(count) {}
    void arrive() {
        std::unique_lock<std::mutex> lk(m_);
        if (--count_ == 0) cv_.notify_all();
    }
    void wait() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [&] { return count_ == 0; });
    }
private:
    std::mutex m_;
    std::condition_variable cv_;
    int count_;
};

// ======== 유틸: Thread-safe Queue (MPMC) ========
template<typename T>
class TSQueue {
public:
    void push(T v) {
        {
            std::lock_guard<std::mutex> lk(m_);
            q_.push(std::move(v));
        }
        cv_.notify_one();
    }
    bool try_pop(T& out) {
        std::lock_guard<std::mutex> lk(m_);
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop();
        return true;
    }
    T wait_pop() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [&] { return !q_.empty(); });
        T v = std::move(q_.front());
        q_.pop();
        return v;
    }
    bool empty() const {
        std::lock_guard<std::mutex> lk(m_);
        return q_.empty();
    }
private:
    mutable std::mutex m_;
    std::condition_variable cv_;
    std::queue<T> q_;
};

// ======== 유틸: 바쁜 대기 (ns) ========
static void busy_wait_ns(long long ns) {
    if (ns <= 0) return;
    auto start = std::chrono::high_resolution_clock::now();
    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
        if (diff >= ns) break;
        // 힌트: 스핀 줄이기
        std::this_thread::yield();
    }
}

// ======== 결과 포맷 ========
struct Result {
    std::string name;
    int64_t     total_ops{};
    double      seconds{};
    double      throughput_ops_per_sec{};
    double      mem_mb{};
};

static void PrintHeader() {
    std::cout << std::left
        << std::setw(34) << "Case"
        << std::setw(14) << "Ops"
        << std::setw(12) << "Time(s)"
        << std::setw(18) << "Throughput(op/s)"
        << std::setw(10) << "Mem(MB)"
        << "\n";
    std::cout << std::string(34 + 14 + 12 + 18 + 10, '-') << "\n";
}

static void PrintRow(const Result& r) {
    std::cout << std::left
        << std::setw(34) << r.name
        << std::setw(14) << r.total_ops
        << std::setw(12) << std::fixed << std::setprecision(3) << r.seconds
        << std::setw(18) << std::fixed << std::setprecision(0) << r.throughput_ops_per_sec
        << std::setw(10) << std::fixed << std::setprecision(1) << r.mem_mb
        << "\n";
}

// ======== 시나리오 1: Same-thread (배치 해제) ========
template<typename AllocT>
static Result Bench_SameThread_allocate_shared(const char* label, int workers, int ops_per_worker, int batch) {
    const int64_t total_ops = int64_t(workers) * int64_t(ops_per_worker);

    CountDownLatch startLatch(workers);
    BenchTimer timer;

    auto mem_before = GetMemoryUsageBytes();

    std::vector<std::thread> ts;
    ts.reserve(workers);

    for (int i = 0; i < workers; ++i) {
        ts.emplace_back([&]() {
            std::vector<std::shared_ptr<TestData_MemoryPool>> stash;
            stash.reserve(batch);

            startLatch.arrive(); // 준비 완료
            startLatch.wait();   // 동시에 시작

            for (int j = 0; j < ops_per_worker; ++j) {
                auto sp = std::allocate_shared<TestData_MemoryPool, AllocT>(AllocT{});
                // 간단 검증 비용 줄이기 (샘플링)
                if ((j & (VERIFY_SAMPLE_RATE - 1)) == 0) {
                    sp->_MyData.fetch_add(1, std::memory_order_relaxed);
                    if (sp->_MyData.load(std::memory_order_relaxed) != 1) {
                        std::cerr << "VERIFY FAIL\n"; std::abort();
                    }
                }
                stash.emplace_back(std::move(sp));
                if ((int)stash.size() >= batch) {
                    // 같은 스레드에서 일괄 해제
                    stash.clear();
                }
            }
            // 남은 것 해제
            stash.clear();
            });
    }

    timer.start();
    for (auto& t : ts) t.join();
    timer.stop();

    auto mem_after = GetMemoryUsageBytes();

    Result r;
    r.name = std::string(label) + " (SameThread, batch=" + std::to_string(batch) + ")";
    r.total_ops = total_ops;
    r.seconds = timer.sec();
    r.throughput_ops_per_sec = double(total_ops) / r.seconds;
    r.mem_mb = double(mem_after) / (1024.0 * 1024.0);
    return r;
}

static Result Bench_SameThread_make_shared(const char* label, int workers, int ops_per_worker, int batch) {
    const int64_t total_ops = int64_t(workers) * int64_t(ops_per_worker);

    CountDownLatch startLatch(workers);
    BenchTimer timer;

    auto mem_before = GetMemoryUsageBytes();

    std::vector<std::thread> ts;
    ts.reserve(workers);

    for (int i = 0; i < workers; ++i) {
        ts.emplace_back([&]() {
            std::vector<std::shared_ptr<TestData_MemoryPool>> stash;
            stash.reserve(batch);

            startLatch.arrive();
            startLatch.wait();

            for (int j = 0; j < ops_per_worker; ++j) {
                auto sp = std::make_shared<TestData_MemoryPool>();
                if ((j & (VERIFY_SAMPLE_RATE - 1)) == 0) {
                    sp->_MyData.fetch_add(1, std::memory_order_relaxed);
                    if (sp->_MyData.load(std::memory_order_relaxed) != 1) {
                        std::cerr << "VERIFY FAIL\n"; std::abort();
                    }
                }
                stash.emplace_back(std::move(sp));
                if ((int)stash.size() >= batch) {
                    stash.clear();
                }
            }
            stash.clear();
            });
    }

    timer.start();
    for (auto& t : ts) t.join();
    timer.stop();

    auto mem_after = GetMemoryUsageBytes();

    Result r;
    r.name = std::string(label) + " (SameThread, batch=" + std::to_string(batch) + ")";
    r.total_ops = total_ops;
    r.seconds = timer.sec();
    r.throughput_ops_per_sec = double(total_ops) / r.seconds;
    r.mem_mb = double(mem_after) / (1024.0 * 1024.0);
    return r;
}

// ======== 시나리오 2/3: Cross-thread (해제 스레드 분리, 지연 옵션) ========
template<typename AllocT>
static Result Bench_CrossThread_allocate_shared(const char* label,
    int alloc_threads,
    int free_threads,
    int ops_per_alloc_thread,
    long long delay_free_ns)
{
    const int64_t total_ops = int64_t(alloc_threads) * int64_t(ops_per_alloc_thread);

    TSQueue<std::shared_ptr<TestData_MemoryPool>> q;
    std::atomic<int64_t> produced{ 0 };
    std::atomic<int64_t> consumed{ 0 };

    CountDownLatch startLatch(alloc_threads + free_threads);

    std::vector<std::thread> allocers;
    std::vector<std::thread> freers;
    allocers.reserve(alloc_threads);
    freers.reserve(free_threads);

    BenchTimer timer;
    auto mem_before = GetMemoryUsageBytes();

    // Producer(s)
    for (int i = 0; i < alloc_threads; ++i) {
        allocers.emplace_back([&]() {
            startLatch.arrive();
            startLatch.wait();

            for (int j = 0; j < ops_per_alloc_thread; ++j) {
                auto sp = std::allocate_shared<TestData_MemoryPool, AllocT>(AllocT{});
                if ((j & (VERIFY_SAMPLE_RATE - 1)) == 0) {
                    sp->_MyData.fetch_add(1, std::memory_order_relaxed);
                    if (sp->_MyData.load(std::memory_order_relaxed) != 1) {
                        std::cerr << "VERIFY FAIL\n"; std::abort();
                    }
                }
                q.push(std::move(sp)); // 파기 책임을 consumer에게 완전 이관
                produced.fetch_add(1, std::memory_order_relaxed);
            }
            });
    }

    // Consumer(s)
    for (int i = 0; i < free_threads; ++i) {
        freers.emplace_back([&]() {
            startLatch.arrive();
            startLatch.wait();

            while (true) {
                if (consumed.load(std::memory_order_relaxed) >= total_ops) break;

                std::shared_ptr<TestData_MemoryPool> sp;
                if (q.try_pop(sp)) {
                    // 지연 해제 시뮬레이션
                    if (delay_free_ns > 0) busy_wait_ns(delay_free_ns);
                    sp.reset(); // 여기서 파괴 → 해제 스레드에서 destructor 실행
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
                else {
                    // 생산이 끝났고 큐가 비었으면 종료
                    if (produced.load(std::memory_order_relaxed) >= total_ops && q.empty()) {
                        break;
                    }
                    std::this_thread::yield();
                }
            }
            });
    }

    timer.start();
    for (auto& t : allocers) t.join();
    for (auto& t : freers)  t.join();
    timer.stop();

    auto mem_after = GetMemoryUsageBytes();

    Result r;
    std::string tag = " (CrossThread";
    if (delay_free_ns > 0) tag += ", delay=" + std::to_string(delay_free_ns) + "ns";
    tag += ")";
    r.name = std::string(label) + tag;
    r.total_ops = total_ops;
    r.seconds = timer.sec();
    r.throughput_ops_per_sec = double(total_ops) / r.seconds;
    r.mem_mb = double(mem_after) / (1024.0 * 1024.0);
    return r;
}

static Result Bench_CrossThread_make_shared(const char* label,
    int alloc_threads,
    int free_threads,
    int ops_per_alloc_thread,
    long long delay_free_ns)
{
    const int64_t total_ops = int64_t(alloc_threads) * int64_t(ops_per_alloc_thread);

    TSQueue<std::shared_ptr<TestData_MemoryPool>> q;
    std::atomic<int64_t> produced{ 0 };
    std::atomic<int64_t> consumed{ 0 };

    CountDownLatch startLatch(alloc_threads + free_threads);

    std::vector<std::thread> allocers;
    std::vector<std::thread> freers;
    allocers.reserve(alloc_threads);
    freers.reserve(free_threads);

    BenchTimer timer;
    auto mem_before = GetMemoryUsageBytes();

    for (int i = 0; i < alloc_threads; ++i) {
        allocers.emplace_back([&]() {
            startLatch.arrive();
            startLatch.wait();

            for (int j = 0; j < ops_per_alloc_thread; ++j) {
                auto sp = std::make_shared<TestData_MemoryPool>();
                if ((j & (VERIFY_SAMPLE_RATE - 1)) == 0) {
                    sp->_MyData.fetch_add(1, std::memory_order_relaxed);
                    if (sp->_MyData.load(std::memory_order_relaxed) != 1) {
                        std::cerr << "VERIFY FAIL\n"; std::abort();
                    }
                }
                q.push(std::move(sp));
                produced.fetch_add(1, std::memory_order_relaxed);
            }
            });
    }

    for (int i = 0; i < free_threads; ++i) {
        freers.emplace_back([&]() {
            startLatch.arrive();
            startLatch.wait();

            while (true) {
                if (consumed.load(std::memory_order_relaxed) >= total_ops) break;

                std::shared_ptr<TestData_MemoryPool> sp;
                if (q.try_pop(sp)) {
                    if (delay_free_ns > 0) busy_wait_ns(delay_free_ns);
                    sp.reset();
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
                else {
                    if (produced.load(std::memory_order_relaxed) >= total_ops && q.empty()) {
                        break;
                    }
                    std::this_thread::yield();
                }
            }
            });
    }

    timer.start();
    for (auto& t : allocers) t.join();
    for (auto& t : freers)  t.join();
    timer.stop();

    auto mem_after = GetMemoryUsageBytes();

    Result r;
    std::string tag = " (CrossThread";
    if (delay_free_ns > 0) tag += ", delay=" + std::to_string(delay_free_ns) + "ns";
    tag += ")";
    r.name = std::string(label) + tag;
    r.total_ops = total_ops;
    r.seconds = timer.sec();
    r.throughput_ops_per_sec = double(total_ops) / r.seconds;
    r.mem_mb = double(mem_after) / (1024.0 * 1024.0);
    return r;
}

// ======== 메인: 케이스 실행 ========


void StartMemoryPool() {
    SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)-1); // 모든 코어 사용 (선택)

    std::vector<Result> results;
    results.reserve(16);

    // 1) Same-thread (make_shared / allocate_shared)
    results.push_back(Bench_SameThread_make_shared("make_shared", SAME_THREAD_WORKERS, OPS_PER_SAME_WORKER, SAME_THREAD_BATCH));
    results.push_back(Bench_SameThread_allocate_shared<MyAllocator<TestData_MemoryPool>>("allocate_shared(PoolAlloc)", SAME_THREAD_WORKERS, OPS_PER_SAME_WORKER, SAME_THREAD_BATCH));

    // 2) Cross-thread 즉시 해제
    results.push_back(Bench_CrossThread_make_shared("make_shared", THREAD_ALLOCERS, THREAD_FREERS, OPS_PER_ALLOC_THREAD, DELAY_NS_NONE));
    results.push_back(Bench_CrossThread_allocate_shared<MyAllocator<TestData_MemoryPool>>("allocate_shared(PoolAlloc)", THREAD_ALLOCERS, THREAD_FREERS, OPS_PER_ALLOC_THREAD, DELAY_NS_NONE));

    // 3) Cross-thread 지연 해제 (소폭/중간)
    results.push_back(Bench_CrossThread_make_shared("make_shared", THREAD_ALLOCERS, THREAD_FREERS, OPS_PER_ALLOC_THREAD, DELAY_NS_SMALL));
    results.push_back(Bench_CrossThread_allocate_shared<MyAllocator<TestData_MemoryPool>>("allocate_shared(PoolAlloc)", THREAD_ALLOCERS, THREAD_FREERS, OPS_PER_ALLOC_THREAD, DELAY_NS_SMALL));

    results.push_back(Bench_CrossThread_make_shared("make_shared", THREAD_ALLOCERS, THREAD_FREERS, OPS_PER_ALLOC_THREAD, DELAY_NS_MEDIUM));
    results.push_back(Bench_CrossThread_allocate_shared<MyAllocator<TestData_MemoryPool>>("allocate_shared(PoolAlloc)", THREAD_ALLOCERS, THREAD_FREERS, OPS_PER_ALLOC_THREAD, DELAY_NS_MEDIUM));

    // 출력
    PrintHeader();
    for (auto& r : results) {
        PrintRow(r);
    }

}
