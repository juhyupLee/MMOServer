#pragma once

template<typename T>
class Singleton
{
private:
	inline static T* g_instance{nullptr};
	inline static std::recursive_mutex g_lock{ };

protected:
	Singleton() = default;

public:
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;

	virtual ~Singleton() = default;

public:
	static T* GetInstance()
	{
		if (g_instance == nullptr)
		{
			return CreateInstance();				
		}
		return g_instance;
	}

private:
	static T* CreateInstance()
	{
		std::lock_guard<std::recursive_mutex> guard(g_lock);

		if (g_instance == nullptr)
		{
			g_instance = new T();
		}
		return g_instance;
	}

};

