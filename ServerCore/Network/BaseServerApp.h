#pragma once
class BaseServerApp
{
public:
	BaseServerApp();
	virtual ~BaseServerApp();

	virtual bool Initialize() = 0;
	virtual bool Start() = 0;
	virtual void Run() = 0;
	virtual void Release() = 0;
};

inline BaseServerApp::BaseServerApp()
{
}

inline BaseServerApp::~BaseServerApp()
{
}
