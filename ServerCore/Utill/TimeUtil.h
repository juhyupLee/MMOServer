#pragma once
class DateTime
{
	friend class DBConnector;
	int64_t epochTime{ 0 };
public:
	DateTime() = default;
	DateTime(int64_t epochTime);
	int64_t ToEpochTime() const;
	operator int64_t() const;
};

using LocalTimePoint = std::chrono::time_point<std::chrono::local_t, std::chrono::milliseconds>;

class TimeUtil
{
	friend class DateTime;
	friend class DBConnector;

	
	
	static double EpochTimeToVariantTime(const int64_t epochTime);
private:
	static LocalTimePoint FromEpochTime(int64_t epochTime);

public:
	static int64_t VariantTimeToEpochTime(const std::string& timeStamp);
	inline static int32_t m_spanTime{ 0 };
	inline static std::optional<LocalTimePoint> NowLocalTimePoint;
	static void FixNow();
	static LocalTimePoint Now();
	static int64_t ToEpochTime(const LocalTimePoint& timePoint);
	static int64_t GetEpochTime(int64_t spanTime = 0);
	template<typename ...Duration>
	static int64_t GetEpochTime(const std::chrono::duration<Duration...>& spanTime);
	static int64_t GetEpochTimeForNetwork(int64_t spanTime = 0);
	template<typename ...Duration>
	static int64_t ToMilliseconds(const std::chrono::duration<Duration...>& spanTime);
	static std::string EpochTimeToString(int64_t epochTime);
	static int32_t GetCurrentHourAndMinute();
	static int64_t GetResetTime(int32_t hour, int32_t minute);
	static int64_t GetNextResetTime(int32_t hour, int32_t minute);
};

template<typename ...Duration>
int64_t TimeUtil::GetEpochTime(const std::chrono::duration<Duration...>& spanTime)
{
	return ToEpochTime(Now() + spanTime);
}

template <typename ...Duration>
int64_t TimeUtil::ToMilliseconds(const std::chrono::duration<Duration...>& timeDuration)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(timeDuration).count();
}

