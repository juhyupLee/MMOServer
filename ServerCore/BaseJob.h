#pragma once
class BaseJob
{
private:

public:
	virtual void Excute(std::function<void(int64_t, MessageHolderPtr)>& dispatch) = 0;
};