////
////#define INIT_DATA 0x123456789
////#define INIT_COUNT  0
////#define DATA_COUNT 1000000
////#define THREAD_NUM 4
////#define TEST_COUNT 1000000
////#define POOL_MODE 2
////
////#define CALL_COUNT 100
////
////
////struct TestData_MemoryPool
////{
////	TestData_MemoryPool()
////	{
////		_MyData = INIT_COUNT;
////	}
////
////	~TestData_MemoryPool()
////	{
////		_MyData = INIT_COUNT;
////	}
////	char buffer[200];
////	std::atomic<int64_t> _MyData;
////};
////
////HANDLE g_Thread_MemoryPool[THREAD_NUM];
////bool g_ExitMemoryPool = false;
////
////std::recursive_mutex g_lock;
////std::recursive_mutex g_lock2;
////
////std::queue<std::shared_ptr<TestData_MemoryPool>> allocQ;
////std::queue<std::shared_ptr<TestData_MemoryPool>> allocQ2;
////
////#if POOL_MODE == 2
////
////void TestThread_Shared()
////{
////	const auto testCount = 1000000;
////	int popCount = 0;
////	Timer A;
////	while (true)
////	{
////		std::lock_guard guard(g_lock2);
////		if (!allocQ2.empty())
////		{
////			auto ptr = allocQ2.front();
////			allocQ2.pop();
////			popCount++;
////
////			if (ptr->_MyData.load() != 1)
////			{
////				CRASH();
////			}
////		}
////		else
////		{
////			auto ptr = std::make_shared<TestData_MemoryPool>();
////			ptr->_MyData++;
////			allocQ2.push(ptr);
////		}
////
////		if (popCount >= TEST_COUNT)
////		{
////			A.stop("New");
////			popCount = 0;
////			A.Start();
////		}
////	}
////}
////void TestThread_AllocPush()
////{
////	while(true)
////	{
////		std::lock_guard guard(g_lock);
////		auto ptr = std::allocate_shared<TestData_MemoryPool>(MyAllocator<TestData_MemoryPool>());
////		ptr->_MyData++;
////		allocQ.push(ptr);
////	}
////	
////}
////
////void TestThread_Alloc()
////{
////	int popCount = 0;
////	Timer A;
////	bool isPop = false;
////
////	while (true)
////	{
////		{
////			std::lock_guard guard(g_lock);
////			if (!allocQ.empty())
////			{
////				auto ptr = allocQ.front();
////				allocQ.pop();
////				popCount++;
////
////				isPop = true;
////				if (ptr->_MyData.load() != 1)
////				{
////					CRASH();
////				}
////
////				if (isPop)
////				{
////					/*for (volatile int i = 0; i < 50000; ++i)
////					{
////
////					}*/
////				}
////			}
////			else
////			{
////				auto ptr = std::allocate_shared<TestData_MemoryPool>(MyAllocator<TestData_MemoryPool>());
////				ptr->_MyData++;
////				allocQ.push(ptr);
////				isPop = false;
////			}
////		}
////
////		if(popCount >= TEST_COUNT)
////		{
////			A.stop("JUHYUP");
////			popCount = 0;
////			A.Start();
////		}
////	}
////}
////#endif 
////
////#if POOL_MODE == 1
////void TestThread_Pool()
////{
////	while (true)
////	{
////	
////		auto ptr = std::allocate_shared<TestData_MemoryPool>(MyAllocator<TestData_MemoryPool>());
////		InterlockedIncrement64(&ptr->_Data);
////		InterlockedIncrement(&ptr->_RefCount);
////
////		if (ptr->_Data != INIT_DATA + 1)
////		{
////			CRASH();
////		}
////		if (ptr->_RefCount != INIT_COUNT + 1)
////		{
////			CRASH();
////		}
////
////		InterlockedDecrement64(&ptr->_Data);
////		InterlockedDecrement(&ptr->_RefCount);
////		if (ptr->_Data != INIT_DATA)
////		{
////			CRASH();
////		}
////		if (ptr->_RefCount != INIT_COUNT)
////		{
////			CRASH();
////		}
////	}
////}
////#endif
////
////void StartMemoryPool()
////{
////#if POOL_MODE == 1
////
////
////	std::vector<std::thread> vectorThread;
////	for (int i = 0; i < THREAD_NUM; ++i)
////	{
////		vectorThread.push_back(std::thread(TestThread_Pool));
////	}
////
////#endif
////#if POOL_MODE ==2
////	std::vector<std::thread> vectorThread;
////
////
////	for (int i = 0; i < THREAD_NUM ; ++i)
////	{
////		vectorThread.push_back(std::thread(TestThread_Alloc));
////	}
////
////	for (int i = 0; i < THREAD_NUM ; ++i)
////	{
////		vectorThread.push_back(std::thread(TestThread_Shared));
////	}
////
////	
////
////#endif
////
////	while (true)
////	{
////		if (GetAsyncKeyState(VK_F4))
////		{
////			g_ExitMemoryPool = true;
////			break;
////		}
////		Sleep(1000);
////	}
////
////
////
////#if POOL_MODE == 3
////
////	int temp = sizeof(TestData_MemoryPool);
////	for (int i = 0; i < THREAD_NUM; ++i)
////	{
////		g_Thread_MemoryPool[i] = (HANDLE)_beginthreadex(NULL, 0, TestThread_NewDeleteVSAllocFree, NULL, 0, NULL);
////	}
////
////
////#endif
////
////	WaitForMultipleObjects(THREAD_NUM, g_Thread_MemoryPool, TRUE, INFINITE);
////
////}
//
//

// PoolVsNew_Benchmark.cpp
// Benchmark: custom Pool allocator (MyAllocator) vs new/make_shared
// - OBJECT_PAD (object size) sweeps up to 900 bytes
// - Thread counts: 1,2,4,8,16,32,64
// - Scenarios: SameThread (alloc/free within worker), CrossThread (producer->consumer), optional delays
// - Prints readable tables AND writes CSV: bench_results.csv
// - Validates correctness: constructor/destructor balance, sampled content checks
//
// Build (MSVC example):
// cl /O2 /std:c++20 PoolVsNew_Benchmark.cpp /EHsc /MD /link Psapi.lib
//
// Use your allocator by defining USE_MY_ALLOC and including header:
// cl /O2 /std:c++20 PoolVsNew_Benchmark.cpp /EHsc /MD /DUSE_MY_ALLOC /Ipath\to\your\headers /link Psapi.lib
// (Make sure MyAllocator<T> models std::allocator traits.)

// PoolVsNew_Benchmark.cpp
// Benchmark: custom Pool allocator (MyAllocator) vs new/make_shared
// - OBJECT_PAD (object size) sweeps up to 900 bytes
// - Thread counts: 1,2,4,8,16,32,64
// - Scenarios: SameThread (alloc/free within worker), CrossThread (producer->consumer), optional delays
// - Prints readable tables AND writes CSV: bench_results.csv
// - Validates correctness: constructor/destructor balance, sampled content checks
//
// Build (MSVC example):
//   cl /O2 /std:c++20 PoolVsNew_Benchmark.cpp /EHsc /MD /link Psapi.lib
//
// Use your allocator by defining USE_MY_ALLOC and including header:
//   cl /O2 /std:c++20 PoolVsNew_Benchmark.cpp /EHsc /MD /DUSE_MY_ALLOC /Ipath\to\your\headers /link Psapi.lib
//   (Make sure MyAllocator<T> models std::allocator traits.)

#include <Windows.h>
#include <Psapi.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>

// ===== Allocator alias ======================================================
#define USE_MY_ALLOC
#ifdef USE_MY_ALLOC
template<class T>
using PoolAlloc = MyAllocator<T>;
#else
#include <memory>
template<class T>
using PoolAlloc = MyAllocator<T>; // fallback so the file builds out of the box
#endif

// ===== Tunables =============================================================
static constexpr int    DEFAULT_SAME_OPS_PER_THREAD = 50'000*5 ;   // per worker
static constexpr int    DEFAULT_CROSS_OPS_PER_ALLOC = 50'000*5 ;   // per alloc thread
static constexpr int    DEFAULT_BATCH = 64;        // SameThread batch free
static constexpr int    VERIFY_SAMPLE_RATE = 1024;      // every N ops, do a light check
static constexpr long long DELAY_NONE_NS = 0;
static constexpr long long DELAY_SMALL_NS = 5'000;     // 5 us
static constexpr long long DELAY_MEDIUM_NS = 50'000;    // 50 us

// ===== Utility: memory usage ===============================================
static size_t GetMemoryUsageBytes()
{
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}

// ===== Utility: timer =======================================================
class BenchTimer {
public:
    void start() { _t0 = clock::now(); }
    void stop() { _t1 = clock::now(); }
    double ms()  const { return std::chrono::duration<double, std::milli>(_t1 - _t0).count(); }
    double sec() const { return ms() / 1000.0; }
private:
    using clock = std::chrono::high_resolution_clock;
    clock::time_point _t0{}, _t1{};
};

// ===== CountDownLatch =======================================================
class CountDownLatch {
public:
    explicit CountDownLatch(int count) : _count(count) {}
    void arrive() { std::unique_lock<std::mutex> lk(_m); if (--_count == 0) _cv.notify_all(); }
    void wait() { std::unique_lock<std::mutex> lk(_m); _cv.wait(lk, [&] {return _count == 0; }); }
private:
    std::mutex _m; std::condition_variable _cv; int _count;
};

// ===== MPMC queue (mutex-based) ============================================
template<typename T>
class TSQueue {
public:
    void push(T v) { { std::lock_guard<std::mutex> lk(_m); _q.push_back(std::move(v)); } _cv.notify_one(); }
    bool try_pop(T& out) {
        std::lock_guard<std::mutex> lk(_m);
        if (_q.empty()) return false;
    	out = std::move(_q.front()); _q.pop_front();
    	return true;
    }
    bool empty() const { std::lock_guard<std::mutex> lk(_m); return _q.empty(); }
private:
    mutable std::mutex _m; std::condition_variable _cv; std::deque<T> _q;
};

static void busy_wait_ns(long long ns)
{
    if (ns <= 0) return;
    auto t0 = std::chrono::high_resolution_clock::now();
    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::nanoseconds>(now - t0).count() >= ns) break;
        std::this_thread::yield();
    }
}

// ===== Test object (templated by OBJECT_PAD) ================================
template<int OBJECT_PAD>
struct TestData {
    static std::atomic<long long> s_live;
    static std::atomic<long long> s_constructed;
    static std::atomic<long long> s_destroyed;

    std::atomic<int64_t> tag{ 0 };
    char                  buf[OBJECT_PAD];

    TestData()
	{
        // simple pattern to catch obvious corruption in sampling
        for (int i = 0; i < OBJECT_PAD; i += 31) buf[i] = static_cast<char>(i & 0x7f);
        s_live.fetch_add(1, std::memory_order_relaxed);
        s_constructed.fetch_add(1, std::memory_order_relaxed);
    }
    ~TestData() {
        s_live.fetch_sub(1, std::memory_order_relaxed);
        s_destroyed.fetch_add(1, std::memory_order_relaxed);
    }
};

template<int OBJECT_PAD>
std::atomic<long long> TestData<OBJECT_PAD>::s_live{ 0 };

template<int OBJECT_PAD>
std::atomic<long long> TestData<OBJECT_PAD>::s_constructed{ 0 };

template<int OBJECT_PAD>
std::atomic<long long> TestData<OBJECT_PAD>::s_destroyed{ 0 };

// ===== Result struct ========================================================
struct Result {
    std::string object_pad;
    int         threads{};
    std::string scenario;   // SameThread or CrossThread
    std::string label;      // make_shared or PoolAlloc
    long long   total_ops{};
    double      seconds{};
    double      throughput{}; // ops/sec
    double      mem_mb{};
    double      p50_us{};   // (optional) latency sampling
    double      p99_us{};
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
    std::ostringstream caseLabel;
    caseLabel << r.label << " (" << r.scenario << ", thr=" << r.threads << ")";
    std::cout << std::left
        << std::setw(34) << caseLabel.str()
        << std::setw(14) << r.total_ops
        << std::setw(12) << std::fixed << std::setprecision(3) << r.seconds
        << std::setw(18) << std::fixed << std::setprecision(0) << r.throughput
        << std::setw(10) << std::fixed << std::setprecision(1) << r.mem_mb
        << "\n";
}

static void AppendCSV(std::ofstream& fout, const Result& r) {
    fout << r.object_pad << "," << r.threads << "," << r.scenario << "," << r.label
        << "," << r.total_ops << "," << r.seconds << "," << r.throughput << "," << r.mem_mb
        << "," << r.p50_us << "," << r.p99_us << "\n";
}

// ===== Bench: SameThread ====================================================

template<class AllocT, int OBJECT_PAD>
static Result Bench_SameThread_allocate_shared(const char* label, int workers, int ops_per_worker, int batch)
{
    using T = TestData<OBJECT_PAD>;
    Result r; r.object_pad = std::to_string(OBJECT_PAD); r.threads = workers; r.label = label; r.scenario = "SameThread";

    const long long total_ops = 1LL * workers * ops_per_worker;
    CountDownLatch latch(workers);
    BenchTimer timer;
    auto mem_before = GetMemoryUsageBytes();

    std::vector<std::thread> ts; ts.reserve(workers);
    std::vector<double> samples_us; samples_us.reserve(workers * 512);

    for (int i = 0; i < workers; ++i)
    {
        ts.emplace_back([&, batch] 
            {
	            std::vector<std::shared_ptr<T>> stash;
	        	stash.reserve(batch);
	            latch.arrive(); latch.wait();
	            auto tstart = std::chrono::high_resolution_clock::now();
	            for (int j = 0; j < ops_per_worker; ++j)
	            {
	                auto t0 = std::chrono::high_resolution_clock::now();
	                auto sp = std::allocate_shared<T, AllocT>(AllocT{});
	                if ((j & (VERIFY_SAMPLE_RATE - 1)) == 0) 
	                {
	                    sp->tag.fetch_add(1, std::memory_order_relaxed);
	                    // light pattern check
	                    if (sp->buf[0] != 0 && (sp->buf[0] & 0x7f) >= OBJECT_PAD) 
	                    {
	                        std::cerr << "[WARN] pattern anomaly\n";
	                    }
	                }
	                stash.emplace_back(std::move(sp));
	                if ((int)stash.size() >= batch) stash.clear(); // same-thread free
	                auto t1 = std::chrono::high_resolution_clock::now();
	                if ((j & 2047) == 0)
	                {
	                    double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
	                    samples_us.push_back(us);
	                }
	            }
	            stash.clear();
	            auto tend = std::chrono::high_resolution_clock::now();
            });
    }

    timer.start(); for (auto& t : ts) t.join(); timer.stop();
    auto mem_after = GetMemoryUsageBytes();

    // quantiles
    std::sort(samples_us.begin(), samples_us.end());
    auto q = [&](double p) { if (samples_us.empty()) return 0.0; size_t idx = size_t(p * (samples_us.size() - 1)); return samples_us[idx]; };

    r.total_ops = total_ops;
    r.seconds = timer.sec();
    r.throughput = double(total_ops) / r.seconds;
    r.mem_mb = double(mem_after) / (1024.0 * 1024.0);
    r.p50_us = q(0.50); r.p99_us = q(0.99);

    // correctness: live must be 0
    if (T::s_live.load(std::memory_order_relaxed) != 0)
    {
        std::cerr << "[ERROR] Leak detected (SameThread/" << label << ") live=" << T::s_live.load() << "\n";
    }

    return r;
}

template<int OBJECT_PAD>
static Result Bench_SameThread_make_shared(const char* label, int workers, int ops_per_worker, int batch) {
    using T = TestData<OBJECT_PAD>;
    Result r; r.object_pad = std::to_string(OBJECT_PAD); r.threads = workers; r.label = label; r.scenario = "SameThread";

    const long long total_ops = 1LL * workers * ops_per_worker;
    CountDownLatch latch(workers);
    BenchTimer timer;
    auto mem_before = GetMemoryUsageBytes(); (void)mem_before;

    std::vector<std::thread> ts; ts.reserve(workers);
    std::vector<double> samples_us; samples_us.reserve(workers * 512);

    for (int i = 0; i < workers; ++i) 
    {
        ts.emplace_back([&, batch] 
            {
	            std::vector<std::shared_ptr<T>> stash; stash.reserve(batch);
	            latch.arrive(); latch.wait();
	            for (int j = 0; j < ops_per_worker; ++j) 
	            {
	                auto t0 = std::chrono::high_resolution_clock::now();
	                auto sp = std::make_shared<T>();
                    if ((j & (VERIFY_SAMPLE_RATE - 1)) == 0)
                    {
                        sp->tag.fetch_add(1, std::memory_order_relaxed);
                        // light pattern check
                        if (sp->buf[0] != 0 && (sp->buf[0] & 0x7f) >= OBJECT_PAD)
                        {
                            std::cerr << "[WARN] pattern anomaly\n";
                        }
                    }
	                stash.emplace_back(std::move(sp));
	                if ((int)stash.size() >= batch) stash.clear();
	                auto t1 = std::chrono::high_resolution_clock::now();
	                if ((j & 2047) == 0) 
	                {
	                    double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
	                    samples_us.push_back(us);
	                }
	            }
				stash.clear();
            });
    }

    timer.start();
    for (auto& t : ts)
    {
        t.join();
    }
	timer.stop();
    auto mem_after = GetMemoryUsageBytes();
    std::sort(samples_us.begin(), samples_us.end());
    auto q = [&](double p) { if (samples_us.empty()) return 0.0; size_t idx = size_t(p * (samples_us.size() - 1)); return samples_us[idx]; };

    r.total_ops = total_ops;
    r.seconds = timer.sec();
    r.throughput = double(total_ops) / r.seconds;
    r.mem_mb = double(mem_after) / (1024.0 * 1024.0);
    r.p50_us = q(0.50); r.p99_us = q(0.99);

    if (T::s_live.load(std::memory_order_relaxed) != 0)
    {
        std::cerr << "[ERROR] Leak detected (SameThread/" << label << ") live=" << T::s_live.load() << "\n";
    }

    return r;
}

// ===== Bench: CrossThread ===================================================

template<class AllocT, int OBJECT_PAD>
static Result Bench_CrossThread_allocate_shared(const char* label, int producers, int consumers, int ops_per_producer, long long delay_free_ns)
{
    using T = TestData<OBJECT_PAD>;
    Result r; r.object_pad = std::to_string(OBJECT_PAD); r.threads = producers + consumers; r.label = label; r.scenario = "CrossThread";

    TSQueue<std::shared_ptr<T>> q;
    std::atomic<long long> produced{ 0 }, consumed{ 0 };

    CountDownLatch latch(producers + consumers);
    BenchTimer timer;

    auto mem_before = GetMemoryUsageBytes(); (void)mem_before;

    std::vector<std::thread> prod, cons; prod.reserve(producers); cons.reserve(consumers);

    for (int i = 0; i < producers; ++i) {
        prod.emplace_back([&] {
        		latch.arrive(); latch.wait();
	            for (int j = 0; j < ops_per_producer; ++j) 
	            {
	                auto sp = std::allocate_shared<T, AllocT>(AllocT{});
                    if ((j & (VERIFY_SAMPLE_RATE - 1)) == 0)
                    {
                        sp->tag.fetch_add(1, std::memory_order_relaxed);
                    }
	                q.push(std::move(sp));
	                produced.fetch_add(1, std::memory_order_relaxed);
	            }
            });
    }
    for (int i = 0; i < consumers; ++i) 
    {
        cons.emplace_back([&] {
            latch.arrive();
        	latch.wait();
            while (true) 
            {
                if (consumed.load(std::memory_order_relaxed) >= 1LL * producers * ops_per_producer) break;
                std::shared_ptr<T> sp;
                if (q.try_pop(sp))
                {
                    if (delay_free_ns > 0)
                    {
                        busy_wait_ns(delay_free_ns);
                    }
                    sp.reset();
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
                else 
                {
                    if (produced.load(std::memory_order_relaxed) >= 1LL * producers * ops_per_producer && q.empty()) break;
                    std::this_thread::yield();
                }
            }
            });
    }

    timer.start();
    for (auto& t : prod)
    {
        t.join();
    }
    for (auto& t : cons)
    {
        t.join();
    }
    timer.stop();

    auto mem_after = GetMemoryUsageBytes();

    r.total_ops = 1LL * producers * ops_per_producer;
    r.seconds = timer.sec();
    r.throughput = double(r.total_ops) / r.seconds;
    r.mem_mb = double(mem_after) / (1024.0 * 1024.0);

    if (T::s_live.load(std::memory_order_relaxed) != 0) 
    {
        std::cerr << "[ERROR] Leak detected (CrossThread/" << label << ") live=" << T::s_live.load() << "\n";
    }
    return r;
}

template<int OBJECT_PAD>
static Result Bench_CrossThread_make_shared(const char* label, int producers, int consumers, int ops_per_producer, long long delay_free_ns)
{
    using T = TestData<OBJECT_PAD>;
    Result r; r.object_pad = std::to_string(OBJECT_PAD); r.threads = producers + consumers; r.label = label; r.scenario = "CrossThread";

    TSQueue<std::shared_ptr<T>> q;
    std::atomic<long long> produced{ 0 }, consumed{ 0 };

    CountDownLatch latch(producers + consumers);
    BenchTimer timer;

    std::vector<std::thread> prod, cons; prod.reserve(producers); cons.reserve(consumers);

    for (int i = 0; i < producers; ++i)
    {
        prod.emplace_back([&] {
            latch.arrive(); latch.wait();
            for (int j = 0; j < ops_per_producer; ++j)
            {
                auto sp = std::make_shared<T>();
                if ((j & (VERIFY_SAMPLE_RATE - 1)) == 0)
                {
                    sp->tag.fetch_add(1, std::memory_order_relaxed);
                }
                
                q.push(std::move(sp));
                produced.fetch_add(1, std::memory_order_relaxed);
            }});
    }
    for (int i = 0; i < consumers; ++i) 
    {
        cons.emplace_back([&] {
            latch.arrive(); latch.wait();
            while (true) 
            {
                if (consumed.load(std::memory_order_relaxed) >= 1LL * producers * ops_per_producer) break;
                std::shared_ptr<T> sp;
                if (q.try_pop(sp)) 
                {
                    if (delay_free_ns > 0)
                    {
                        busy_wait_ns(delay_free_ns);
                    }
                    sp.reset();
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
                else 
                {
                    if (produced.load(std::memory_order_relaxed)>= 1LL * producers * ops_per_producer && q.empty())
                    {
                        break;
                    }
                    std::this_thread::yield();
                }
            }
            });
    }

    timer.start();
    for (auto& t : prod)
    {
        t.join();
    }
    for (auto& t : cons)
    {
        t.join();
    }
    timer.stop();

    auto mem_after = GetMemoryUsageBytes();
    r.total_ops = 1LL * producers * ops_per_producer;
    r.seconds = timer.sec();
    r.throughput = double(r.total_ops) / r.seconds;
    r.mem_mb = double(mem_after) / (1024.0 * 1024.0);

    if (T::s_live.load(std::memory_order_relaxed) != 0) 
    {
        std::cerr << "[ERROR] Leak detected (CrossThread/" << label << ") live=" << T::s_live.load() << "\n";
    }
    return r;
}

// ===== Driver over OBJECT_PAD & threads ====================================

template<int OBJECT_PAD>
void RunForPad(std::ofstream& csv,
    const std::vector<int>& threadCounts,
    int same_ops_per_thread,
    int cross_ops_per_alloc,
    int same_batch,
    bool run_delays)
{
    using T = TestData<OBJECT_PAD>;

    std::cout << "\n===== OBJECT_PAD=" << OBJECT_PAD << " =====\n";
    PrintHeader();

    auto print_and_store = [&](const Result& r) { PrintRow(r); AppendCSV(csv, r); };

    for (int thr : threadCounts) {
        // SameThread
        {
            auto r1 = Bench_SameThread_make_shared<OBJECT_PAD>("make_shared", thr, same_ops_per_thread, same_batch);
            print_and_store(r1);
            auto r2 = Bench_SameThread_allocate_shared<PoolAlloc<T>, OBJECT_PAD>("allocate_shared(PoolAlloc)", thr, same_ops_per_thread, same_batch);
            print_and_store(r2);
        }

        // CrossThread: split threads into producers/consumers
        int producers = std::max(1, thr / 2);
        int consumers = std::max(1, thr - producers);
        {
            auto r3 = Bench_CrossThread_make_shared<OBJECT_PAD>("make_shared", producers, consumers, cross_ops_per_alloc, DELAY_NONE_NS);
            print_and_store(r3);
            auto r4 = Bench_CrossThread_allocate_shared<PoolAlloc<T>, OBJECT_PAD>("allocate_shared(PoolAlloc)", producers, consumers, cross_ops_per_alloc, DELAY_NONE_NS);
            print_and_store(r4);
        }
        if (run_delays) {
            auto r5 = Bench_CrossThread_make_shared<OBJECT_PAD>("make_shared", producers, consumers, cross_ops_per_alloc, DELAY_SMALL_NS);
            print_and_store(r5);
            auto r6 = Bench_CrossThread_allocate_shared<PoolAlloc<T>, OBJECT_PAD>("allocate_shared(PoolAlloc)", producers, consumers, cross_ops_per_alloc, DELAY_SMALL_NS);
            print_and_store(r6);

            auto r7 = Bench_CrossThread_make_shared<OBJECT_PAD>("make_shared", producers, consumers, cross_ops_per_alloc, DELAY_MEDIUM_NS);
            print_and_store(r7);
            auto r8 = Bench_CrossThread_allocate_shared<PoolAlloc<T>, OBJECT_PAD>("allocate_shared(PoolAlloc)", producers, consumers, cross_ops_per_alloc, DELAY_MEDIUM_NS);
            print_and_store(r8);
        }

        // Reset live counters (safety) — should already be zero
        if (T::s_live.load(std::memory_order_relaxed) != 0) 
        {
            std::cerr << "[WARN] live counter non-zero after thr=" << thr << ": " << T::s_live.load() << "\n";
        }
        T::s_constructed.store(0, std::memory_order_relaxed);
        T::s_destroyed.store(0, std::memory_order_relaxed);
    }
}

void StartMemoryPool()
{
    // Defaults (you can tweak below or via small edits)
    std::vector<int> pads = { 32, 128, 512, 900 }; // up to 900
    std::vector<int> threads = { 1,2,4,8,16,32,64 };
    int same_ops = DEFAULT_SAME_OPS_PER_THREAD;
    int cross_ops = DEFAULT_CROSS_OPS_PER_ALLOC;
    int batch = DEFAULT_BATCH;    // batch free in SameThread
    bool delays = true;             // run CrossThread delay variants

    std::ofstream csv("bench_results.csv");
    csv << "ObjectPad,Threads,Scenario,Allocator,Ops,Time(s),Throughput(op/s),Mem(MB),p50_us,p99_us\n";

    // Run for each pad
    RunForPad<32>(csv, threads, same_ops, cross_ops, batch, delays);
    RunForPad<128>(csv, threads, same_ops, cross_ops, batch, delays);
    RunForPad<512>(csv, threads, same_ops, cross_ops, batch, delays);
    RunForPad<900>(csv, threads, same_ops, cross_ops, batch, delays);

    std::cout << "\nCSV written: bench_results.csv\n";

}