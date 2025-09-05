#pragma once
#include "JobQueue.h"

class JobDispatcher
{
protected: 
	volatile bool m_running{false};
	std::vector<std::thread> m_threads{ };

	std::deque<std::shared_ptr<JobQueue>> m_activeJobQueue;
	std::condition_variable m_signal;
	std::mutex m_lock{ };
public:
	JobDispatcher(int32_t threadCount = 1);
	virtual ~JobDispatcher();
public:
	virtual void PushJobQueue(const std::shared_ptr<JobQueue>& dbQueue);
	virtual std::shared_ptr<JobQueue> PopJobQueue();

	void Run(std::function<void()> dispatch);

};
