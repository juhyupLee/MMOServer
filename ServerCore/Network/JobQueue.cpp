#include "JobQueue.h"
#include "JobDispatcher.h"

JobQueue::JobQueue(JobDispatcher* jobDispatcher)
    : m_jobDispatcher(jobDispatcher)
{
}

JobDispatcher* JobQueue::GetJobDispatcher()
{
	return m_jobDispatcher;
}


void JobQueue::Push(std::shared_ptr<BaseJob>& job)
{
	std::lock_guard<std::mutex> guard(m_lock);
	m_dataQueue.emplace_back(job);
	if (m_dbJobCount.fetch_add(1) == 0)
	{
		m_jobDispatcher->PushJobQueue(this->shared_from_this());
	}
}

bool JobQueue::Pop(std::deque<std::shared_ptr<BaseJob>>& messages)
{
	std::lock_guard<std::mutex> guard(m_lock);
	if (m_dataQueue.empty() == false)
	{
		messages.swap(m_dataQueue);
		return true;
	}
	return false;
}

int32_t JobQueue::Decrement(int32_t size)
{
	return m_dbJobCount.fetch_sub(size) - size;
}
