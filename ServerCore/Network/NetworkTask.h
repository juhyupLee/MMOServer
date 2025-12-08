#pragma once

class NetworkServer;
struct NetworkTask : OVERLAPPED
{
	std::shared_ptr<NetworkSession> m_owner{ nullptr };
	EStep m_step{ EStep::None };
	virtual bool Run(bool result, DWORD transferred) = 0;
};

struct NetworkTaskClose : NetworkTask
{
	std::unordered_set<SessionID> m_sessionIDs{ };
	virtual bool Run(bool result, DWORD transferred) override;
};

struct NetworkTaskListen : NetworkTask
{
	int32_t m_port{ 0 };
	JobDispatcher* m_jobDispatcher{nullptr};
	virtual bool Run(bool result, DWORD transferred) override;
};

struct NetworkTaskAcceptIO : NetworkTask
{
	SOCKET m_clientSocket{ INVALID_SOCKET };
	char m_address[MAX_ADDRESS_SIZE]{ };
	virtual bool Run(bool result, DWORD transferred) override;

	bool Start();
	bool Complete(bool result);
};

struct NetworkTaskNewUser : NetworkTask
{
	std::string m_ip{ };
	int32_t m_port{ 0 };
	SOCKET m_socket{ INVALID_SOCKET };
	JobDispatcher* m_jobDispatcher{ nullptr };
	virtual bool Run(bool result, DWORD transferred) override;
};


struct NetworkTaskChange : NetworkTask
{
	SessionID m_sessionID{ 0 };
	HANDLE m_key{ INVALID_HANDLE_VALUE };

	virtual bool Run(bool result, DWORD transferred) override;
};

struct NetworkTaskReceiveIO : NetworkTask
{
	virtual bool Run(bool result, DWORD transferred) override;

	bool Start();
	bool Complete(bool result, DWORD transferred);
};

struct NetworkTaskSend : NetworkTask
{
	DWORD m_totalSize{ 0 };

	virtual bool Run(bool result, DWORD transferred) override;

	bool Start();
	bool Complete(bool result, DWORD transferred);
};

struct NetworkTaskConnect : NetworkTask
{
	std::string m_ip{ };
	int32_t m_port{ 0 };
	JobDispatcher* m_jobDispatcher{ nullptr };
	virtual bool Run(bool result, DWORD transferred) override;
};

struct NetworkTaskConnectIO : NetworkTask
{
	virtual bool Run(bool result, DWORD transferred) override;

	bool Start();
	bool Complete(bool result);
};
