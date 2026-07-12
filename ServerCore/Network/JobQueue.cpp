#include "JobQueue.h"
#include "JobDispatcher.h"

#include <algorithm>
#include <limits>
#include <utility>

JobQueue::JobQueue(JobDispatcher* jobDispatcher, std::size_t maxPendingJobCount)
	: JobQueue(
		jobDispatcher == nullptr ? nullptr : jobDispatcher->GetHandle(),
		maxPendingJobCount)
{
}

JobQueue::JobQueue(
	std::shared_ptr<JobDispatcherHandle> jobDispatcherHandle,
	std::size_t maxPendingJobCount)
	: m_jobDispatcherHandle(std::move(jobDispatcherHandle))
	, m_maxPendingJobCount(static_cast<int32_t>(std::clamp<std::size_t>(
		maxPendingJobCount,
		1,
		static_cast<std::size_t>(std::numeric_limits<int32_t>::max()))))
{
}

JobDispatcher* JobQueue::GetJobDispatcher()
{
	if (m_jobDispatcherHandle == nullptr)
	{
		return nullptr;
	}

	std::lock_guard handleGuard(m_jobDispatcherHandle->lock);
	return m_jobDispatcherHandle->dispatcher;
}


bool JobQueue::Push(const std::shared_ptr<BaseJob>& job)
{
	if (job == nullptr || m_jobDispatcherHandle == nullptr)
	{
		return false;
	}

	std::lock_guard<std::mutex> guard(m_lock);
	if (m_dbJobCount.load(std::memory_order_relaxed) >= m_maxPendingJobCount)
	{
		return false;
	}

	// The handle lock keeps the raw dispatcher alive for the complete call.
	// Its destructor clears dispatcher under the same lock before teardown.
	std::lock_guard handleGuard(m_jobDispatcherHandle->lock);
	auto* dispatcher = m_jobDispatcherHandle->dispatcher;
	if (dispatcher == nullptr)
	{
		return false;
	}

	m_dataQueue.emplace_back(job);
	if (m_dbJobCount.fetch_add(1) == 0)
	{
		dispatcher->PushJobQueue(this->shared_from_this());
	}
	return true;
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
