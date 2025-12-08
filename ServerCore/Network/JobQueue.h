#pragma once
class BaseJob;
class JobDispatcher;

class JobQueue :public std::enable_shared_from_this<JobQueue>
{
private:
	JobDispatcher* m_jobDispatcher;
	std::mutex m_lock{ };
	std::deque<std::shared_ptr<BaseJob>> m_dataQueue;
	std::atomic<int32_t> m_dbJobCount{0};
	
public:
	JobQueue(JobDispatcher* jobDispatcher);
public:
	JobDispatcher* GetJobDispatcher();
	void Push(const std::shared_ptr<BaseJob>& job);
	bool Pop(std::deque<std::shared_ptr<BaseJob>>& messages);
    int32_t Decrement(int32_t size);
	//void Push(const T& message);
	

};
