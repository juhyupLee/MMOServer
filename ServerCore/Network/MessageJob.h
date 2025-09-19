#pragma once
#include "BaseJob.h"

class MessageJob : public BaseJob
{
public:
	MessageJob(MessageHolderPtr message, int64_t sessionID)
		:m_message(message), m_sessionID(sessionID)
	{
		int a = 10;
		a;
	}

	~MessageJob();
public:
	void Excute(std::function<void(int64_t, MessageHolderPtr)>& dispatch) override;
	MessageHolderPtr m_message;
	int64_t m_sessionID;
};

inline MessageJob::~MessageJob()
{
	int a = 10;
}

inline void MessageJob::Excute(std::function<void(int64_t, MessageHolderPtr)>& dispatch)
{
	dispatch(m_sessionID, m_message);
}
