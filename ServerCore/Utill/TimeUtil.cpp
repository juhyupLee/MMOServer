
#include "TimeUtil.h"

#include <cstdlib>
#include <format>
#include <sstream>

//============================================================================================================================
DateTime::DateTime(int64_t epochTime)
    : epochTime(epochTime)
{
}

int64_t DateTime::ToEpochTime() const
{
    return epochTime;
}

DateTime::operator int64_t() const
{
    return epochTime;
}


//============================================================================================================================
static constexpr auto OLE_AUTOMATION_BASE_TIME = std::chrono::local_days{std::chrono::year{1899} / std::chrono::month{12} / std::chrono::day{30}};
static constexpr double DAY_TO_MILLISECONDS = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::days{1}).count());


int64_t TimeUtil::VariantTimeToEpochTime(const std::string& timeStamp)
{
    LocalTimePoint tp;  // time_point 역할
    std::istringstream iss(timeStamp);

    // PostgreSQL timestamp format
    iss >> parse("%Y-%m-%d %H:%M:%S", tp);

    return ToEpochTime(tp);
}

double TimeUtil::EpochTimeToVariantTime(const int64_t epochTime)
{
    const auto timePoint = FromEpochTime(epochTime);
    const auto daysPart = std::chrono::floor<std::chrono::days>(timePoint);
    const auto timePart = std::chrono::duration_cast<std::chrono::milliseconds>(timePoint - daysPart).count() / DAY_TO_MILLISECONDS;
    return std::chrono::duration_cast<std::chrono::days>(daysPart - OLE_AUTOMATION_BASE_TIME).count() + timePart;
}

LocalTimePoint TimeUtil::FromEpochTime(int64_t epochTime)
{
    const auto splitSeconds = std::div(epochTime, static_cast<int64_t>(1000));
    const auto timePoint = std::chrono::system_clock::from_time_t(splitSeconds.quot) + std::chrono::milliseconds{splitSeconds.rem};
    return LocalTimePoint{std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch())};
}

void TimeUtil::FixNow()
{
    const auto zoneTime = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()}.get_local_time();
    NowLocalTimePoint = std::chrono::time_point_cast<std::chrono::milliseconds>(zoneTime);
}

LocalTimePoint TimeUtil::Now()
{
    if (NowLocalTimePoint)
    {
        return NowLocalTimePoint.value() + std::chrono::minutes{m_spanTime};
    }
    const auto zoneTime = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()}.get_local_time();
    //std::format("{:%Y-%m-%d %H:%M:%S}", zoneTime);
    return std::chrono::time_point_cast<std::chrono::milliseconds>(zoneTime) + std::chrono::minutes{m_spanTime};
        
}

int64_t TimeUtil::ToEpochTime(const LocalTimePoint& timePoint)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
}

int64_t TimeUtil::GetEpochTime(int64_t spanTime)
{
    return ToEpochTime(Now()) + spanTime;
}

int64_t TimeUtil::GetEpochTimeForNetwork(int64_t spanTime)
{
    const auto zoneTime = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()}.get_local_time();
    return ToEpochTime(std::chrono::time_point_cast<std::chrono::milliseconds>(zoneTime)) + spanTime;
}

std::string TimeUtil::EpochTimeToString(int64_t epochTime)
{
    return std::format("{:%Y-%m-%d %H:%M:%S}", FromEpochTime(epochTime));
}
int32_t TimeUtil::GetCurrentHourAndMinute()
{
    const auto zoneTime = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()}.get_local_time() + std::chrono::minutes{m_spanTime};
    auto sinceTime = zoneTime.time_since_epoch() % std::chrono::days{1};
    std::chrono::hh_mm_ss time{sinceTime};

    return time.hours().count() * 100 + time.minutes().count();
}
int64_t TimeUtil::GetResetTime(int32_t hour, int32_t minute)
{
    auto resetTime = std::chrono::floor<std::chrono::days>(Now()) + std::chrono::hours(hour) + std::chrono::minutes(minute);
    return ToEpochTime(resetTime);
}

int64_t TimeUtil::GetNextResetTime(int32_t hour, int32_t minute)
{
    auto resetTime = std::chrono::floor<std::chrono::days>(Now()) + std::chrono::days(1)+ std::chrono::hours(hour) + std::chrono::minutes(minute);
    return ToEpochTime(resetTime);
}

