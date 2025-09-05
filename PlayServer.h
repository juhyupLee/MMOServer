#pragma once
#include "../ServerCore/Network/JobDispatcher.h"

class PlayServer : public BaseServerApp
{
public:
	JobDispatcher m_main;
	bool Initialize() override;
	bool Start() override;
	void Run() override;
	void Release() override;
};
