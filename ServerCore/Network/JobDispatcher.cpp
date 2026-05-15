#include "JobDispatcher.h"
#include "BaseJob.h"

JobDispatcher::JobDispatcher(std::function<void(int64_t, PacketHolder)> dispatch, int32_t timeout, int32_t threadCount)
{
	m_running = true;
	for (int32_t n = 0; n < threadCount; ++n)
	{
		m_threads.emplace_back([this, dispatch, timeout]
		{
			Run(dispatch, timeout);
		});
	}
}

JobDispatcher::~JobDispatcher()
{
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_running = false;
	}
	m_signal.notify_all();

	for (auto& t : m_threads)
	{
		if (t.joinable())
		{
			t.join();
		}
	}
}

void JobDispatcher::PushJobQueue(const std::shared_ptr<JobQueue>& dbQueue)
{
	{
		std::unique_lock<std::mutex> lockGuard(m_lock);
		m_activeJobQueue.push_back(dbQueue);
	}
	m_signal.notify_one();
}

std::shared_ptr<JobQueue> JobDispatcher::PopJobQueue(int32_t timeout)
{
	std::unique_lock<std::mutex> lockGuard(m_lock);
	if(timeout == 0)
	{
		m_signal.wait(lockGuard, [this](){
			return m_activeJobQueue.empty() == false || m_running == false;
		});
	}
	else
	{
		m_signal.wait_for(lockGuard, std::chrono::microseconds(timeout), [this]() {
			return m_activeJobQueue.empty() == false || m_running == false;
		});
	}

	if (m_activeJobQueue.empty())
	{
		return nullptr;
	}
	auto jobQueue = m_activeJobQueue.front();
	m_activeJobQueue.pop_front();
	return jobQueue;


}

void JobDispatcher::Run(std::function<void(int64_t, PacketHolder)> dispatch, int32_t timeout)
{
	while (m_running)
	{
		auto dbQueue = PopJobQueue(timeout);
		if(dbQueue == nullptr)
		{
			continue;
		}
		std::deque<std::shared_ptr<BaseJob>> messages;
		dbQueue->Pop(messages);

		for (auto& messageJob : messages)
		{
			messageJob->Excute(dispatch);
		}

		if (0 < dbQueue->Decrement(static_cast<int32_t>(messages.size())))
		{
			PushJobQueue(dbQueue);
		}
	}
}
