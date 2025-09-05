#include "JobDispatcher.h"


JobDispatcher::JobDispatcher(int32_t threadCount)
{
	for (int32_t n = 0; n < threadCount; ++n)
	{
		m_threads.emplace_back([this]
			{
				Run([]()
				{
					
				});
			});
	}
}

JobDispatcher::~JobDispatcher()
{
}

void JobDispatcher::PushJobQueue(const std::shared_ptr<JobQueue>& dbQueue)
{
	{
		std::unique_lock<std::mutex> lockGuard(m_lock);
		m_activeJobQueue.push_back(dbQueue);
	}
	m_signal.notify_one();
}

std::shared_ptr<JobQueue> JobDispatcher::PopJobQueue()
{
	std::unique_lock<std::mutex> lockGuard(m_lock);
	m_signal.wait(lockGuard, [this]()
		{
			return m_activeJobQueue.empty() == false;
		});

	auto jobQueue = m_activeJobQueue.front();
	m_activeJobQueue.pop_front();
	return jobQueue;
}

void JobDispatcher::Run(std::function<void()> dispatch)
{
	while (true)
	{
		auto dbQueue = PopJobQueue();
		std::deque<std::shared_ptr<BaseJob>> messages;
		dbQueue->Pop(messages);

		//for (auto& [entryID, messageHolder] : messages)
		//{
		//	dispatch(entryID, messageHolder);
		//}

		if (0 < dbQueue->Decrement(static_cast<int32_t>(messages.size())))
		{
			PushJobQueue(dbQueue);
		}
	}
}
