#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>

class BaseJob;
class JobDispatcher;
struct JobDispatcherHandle;

class JobQueue :public std::enable_shared_from_this<JobQueue>
{
private:
	std::shared_ptr<JobDispatcherHandle> m_jobDispatcherHandle;
	std::mutex m_lock{ };
	std::deque<std::shared_ptr<BaseJob>> m_dataQueue;
	std::atomic<int32_t> m_dbJobCount{0};
	int32_t m_maxPendingJobCount{ 256 };
	
public:
	JobQueue(JobDispatcher* jobDispatcher, std::size_t maxPendingJobCount = 256);
	JobQueue(
		std::shared_ptr<JobDispatcherHandle> jobDispatcherHandle,
		std::size_t maxPendingJobCount = 256);
public:
	JobDispatcher* GetJobDispatcher();
	bool Push(const std::shared_ptr<BaseJob>& job);
	bool Pop(std::deque<std::shared_ptr<BaseJob>>& messages);
    int32_t Decrement(int32_t size);
	int32_t GetPendingJobCount() const noexcept
	{
		return m_dbJobCount.load(std::memory_order_relaxed);
	}
	//void Push(const T& message);
	

};
