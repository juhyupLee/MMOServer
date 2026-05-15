#pragma once

#define _MEMO(s)					#s
#define MEMO(s)						_MEMO(s)

#ifdef _MSC_VER
	#define LOG(fmt, ...)				LogManager::GetInstance()->InfoPrint("[" __FUNCTION__ " "  MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define LOG_DEBUG(fmt, ...)			LogManager::GetInstance()->DebugPrint("[" __FUNCTION__ " "  MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define LOG_INFO(fmt, ...)			LogManager::GetInstance()->InfoPrint("[" __FUNCTION__ " "  MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define LOG_WARN(fmt, ...)			LogManager::GetInstance()->WarningPrint("[" __FUNCTION__ " "  MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define LOG_ERR(fmt, ...)			LogManager::GetInstance()->ErrorPrint("[" __FUNCTION__ " "  MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define LOG_CRI(fmt, ...)			LogManager::GetInstance()->CriticalPrint("[" __FUNCTION__ " "  MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define PERFORMANCE(fmt, ...)		LogManager::GetInstance()->InfoPrint(fmt, ##__VA_ARGS__)
#else
	// GCC/Clang: __FUNCTION__ is not a string literal, so build prefix at runtime
	#define LOG(fmt, ...)				LogManager::GetInstance()->InfoPrint(std::string("[") + __func__ + " " MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define LOG_DEBUG(fmt, ...)			LogManager::GetInstance()->DebugPrint(std::string("[") + __func__ + " " MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define LOG_INFO(fmt, ...)			LogManager::GetInstance()->InfoPrint(std::string("[") + __func__ + " " MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define LOG_WARN(fmt, ...)			LogManager::GetInstance()->WarningPrint(std::string("[") + __func__ + " " MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define LOG_ERR(fmt, ...)			LogManager::GetInstance()->ErrorPrint(std::string("[") + __func__ + " " MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define LOG_CRI(fmt, ...)			LogManager::GetInstance()->CriticalPrint(std::string("[") + __func__ + " " MEMO(__LINE__) "] " fmt, ##__VA_ARGS__)
	#define PERFORMANCE(fmt, ...)		LogManager::GetInstance()->InfoPrint(fmt, ##__VA_ARGS__)
#endif

#ifndef _DEBUG
    #define SPDLOG_NO_EXCEPTIONS
#endif

class LogValue
{
private:
	std::string m_value{ };

public:
	template<typename T>
	LogValue(const T& value)
	{
		std::stringstream stream;
		stream << value;
		m_value = stream.str();
	}

	LogValue(const std::string& value)
	{
		std::regex pattern("\\{.*\\}");
		if (std::regex_match(value, pattern))
		{
			// json object 표현식({})의 경우 가공하지 않음.
			m_value = value;
		}
		else
		{
			// 일반문자열은 작은따옴표로 묶는다.
			m_value = WrapText(value);
		}
	}

	LogValue(const char* value)
		: LogValue(std::string(value))
	{
	}

	operator const char* () const
	{
		return m_value.c_str();
	}

private:
	std::string WrapText(const std::string& text) const
	{
		std::stringstream stream;
		stream << "'" << text << "'";

		return stream.str();
	}
};

class LogManager : public Singleton<LogManager>
{
private:
	int64_t m_updateTime{0};
	int32_t m_packetLogActivate{1};

public:
	void Init();
	void Update();
	void CreateLog();
	void Destroy();
	void OldLogDelete();

	void SetPacketLogActivate(int32_t packetLogActivate);
	int32_t GetPacketLogActivate();

public:
	template <typename T>
	void AddArgs(std::vector<LogValue>& storage, T& value)
	{
		LogValue logValue = ConvertArgToLogValue(value);
		storage.emplace_back(value);
	}

	template <typename T, typename... Arguments>
	void AddArgs(std::vector<LogValue>& storage, T& value, Arguments... args)
	{
		LogValue logValue = ConvertArgToLogValue(value);
		storage.emplace_back(logValue);
		AddArgs(storage, args...);
	}

	template <typename T>
	LogValue ConvertArgToLogValue(T& value)
	{
		return std::move(LogValue(value));
	}

	template <typename... Arguments>
	std::string GetLogText(const std::string& fmt, Arguments... args)
	{
		std::vector<LogValue> storage;
		AddArgs(storage, args...);

		std::string result;
		AppendDetailInfo(result, fmt, storage);

		return result;
	}

	template <typename... Arguments>
	void InfoPrint(const std::string& fmt, Arguments... args)
	{
		SPDLOG_INFO(GetLogText(fmt, args...));
	}

	void InfoPrint(const std::string& fmt)
	{
		SPDLOG_INFO(fmt);
	}

	template <typename... Arguments>
	void DebugPrint(const std::string& fmt, Arguments... args)
	{
		SPDLOG_DEBUG(GetLogText(fmt, args...));
	}

	void DebugPrint(const std::string& fmt)
	{
		SPDLOG_DEBUG(fmt);
	}

	template <typename... Arguments>
	void WarningPrint(const std::string& fmt, Arguments... args)
	{
		SPDLOG_WARN(GetLogText(fmt, args...));
	}

	void WarningPrint(const std::string& fmt)
	{
		SPDLOG_WARN(fmt);
	}

	template <typename... Arguments>
	void ErrorPrint(const std::string& fmt, Arguments... args)
	{
		SPDLOG_ERROR(GetLogText(fmt, args...));
	}

	void ErrorPrint(const std::string& fmt)
	{
		SPDLOG_ERROR(fmt);
	}

	template <typename... Arguments>
	void CriticalPrint(const std::string& fmt, Arguments... args)
	{
		SPDLOG_CRITICAL(GetLogText(fmt, args...));
	}

	void CriticalPrint(const std::string& fmt)
	{
		SPDLOG_CRITICAL(fmt);
	}

	void PrintPacket(const std::string& tag, const std::string& log);

private:
	void AppendDetailInfo(std::string& result, const std::string& fmt, const std::vector<LogValue>& values);
};
