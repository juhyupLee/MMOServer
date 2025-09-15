#pragma once

class UIDGenerator : public Singleton<UIDGenerator>
{
private:
	std::atomic_int64_t m_lastSessionID{1};
public:
	UIDGenerator();
	virtual ~UIDGenerator();

public:
	int64_t GenerateSessionID();
};

