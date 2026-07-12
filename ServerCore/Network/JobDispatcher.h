#pragma once
#include "JobQueue.h"
#include "../Memory/Global.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class JobDispatcher;

struct JobDispatcherHandle
{
	std::mutex lock;
	JobDispatcher* dispatcher{ nullptr };
};

class JobDispatcher
{
protected: 
	std::atomic<bool> m_running{ false };
	std::vector<std::jthread> m_threads{ };
	std::shared_ptr<JobDispatcherHandle> m_handle;

	std::deque<std::shared_ptr<JobQueue>> m_activeJobQueue;
	std::condition_variable m_signal;
	std::mutex m_lock{ };
public:
	JobDispatcher(std::function<void(int64_t, PacketHolder)> dispatch, int32_t timeout,  int32_t threadCount = 1);
	virtual ~JobDispatcher();
public:
	virtual void PushJobQueue(const std::shared_ptr<JobQueue>& dbQueue);
	virtual std::shared_ptr<JobQueue> PopJobQueue(int32_t timeout = 0);
	std::shared_ptr<JobDispatcherHandle> GetHandle() const noexcept { return m_handle; }

private:
	void Run(std::function<void(int64_t, PacketHolder)> dispatch, int32_t timeout);

};
