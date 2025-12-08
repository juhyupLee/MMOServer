#include "LogManager.h"
#include "spdlog/async.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/daily_file_sink.h"


void LogManager::Init()
{
	// 로그 생성
	CreateLog();
}

void LogManager::Update()
{
	auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count();

	if (m_updateTime < now)
	{
		CreateLog();
	}
}

void LogManager::CreateLog()
{
	// 밀리세컨즈를 시간 단위로 변환합니다.
	std::chrono::milliseconds ms(m_updateTime);

	// 시간을 추출합니다.
	std::chrono::system_clock::time_point tp(ms);

	// 시간을 문자열로 변환합니다.
	std::time_t tt = std::chrono::system_clock::to_time_t(tp);
	std::tm timeInfo;
	localtime_s(&timeInfo, &tt);

	// 문자열로 포맷을 지정합니다. YYYYMMDDHH 형식
	std::stringstream ss;
	ss << std::put_time(&timeInfo, "%Y%m%d%H");

	//auto serverName = StringUtil::ReadString("log", "name");
	//auto path = StringUtil::ReadString("log", "path");
	//std::string fileName = path + serverName + "_" + ss.str() + ".txt";

	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	console_sink->set_level(spdlog::level::info);
	console_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [thread %t] [%^%l%$] %v");

	//auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(fileName, 1024 * 1024 * 100, 1000);
	//rotating_sink->set_level(spdlog::level::trace);
	//rotating_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [thread %t] [%^%l%$] %v");

	spdlog::set_default_logger(std::make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list({console_sink})));
	spdlog::set_level(spdlog::level::debug);
	spdlog::flush_on(spdlog::level::debug);

	//m_updateTime = m_updateTime + 3600000;
}

void LogManager::PrintPacket(const std::string& tag, const std::string& log)
{
	std::string result;
	result.append(tag);
	result.append(log);
	SPDLOG_INFO(result);
}

//std::string LogManager::MessageToJson(const uint8_t* buffer)
//{
//	return flatbuffers::FlatBufferToString(buffer + PACKET_HEADER_SIZE, MessageHolder::MiniReflectTypeTable());
//}

//std::string LogManager::MessageToJson(const char* buffer)
//{
//	//return MessageToJson(reinterpret_cast<const uint8_t*>(buffer));
//}
//
//std::string LogManager::MessageToJson(const PacketHolder& messageHolder)
//{
//	flatbuffers::FlatBufferBuilder fbb;
//	auto offset = MessageHolder::Pack(fbb, messageHolder.get());
//	fbb.FinishSizePrefixed(offset);
//	return MessageToJson(fbb.GetBufferPointer());
//}

void LogManager::AppendDetailInfo(std::string& result, const std::string& fmt, const std::vector<LogValue>& values)
{
	auto itFmt = fmt.begin();
	auto endFmt = fmt.end();

	auto itValue = values.begin();
	auto endValue = values.end();

	while (itFmt != endFmt)
	{
		if (*itFmt == '%')
		{
			if (itFmt != endFmt && itValue != endValue)
			{
				result.append(*itValue);
				++itFmt;
				++itValue;
			}
			else
			{
				break;
			}
		}
		else
		{
			result += *itFmt;
			++itFmt;
		}
	}
}

void LogManager::Destroy()
{
	spdlog::shutdown();
}

void LogManager::SetPacketLogActivate(int32_t packetLogActivate)
{
	m_packetLogActivate = packetLogActivate;
}

int32_t LogManager::GetPacketLogActivate()
{
	return m_packetLogActivate;
}

void LogManager::OldLogDelete()
{
	//std::filesystem::path directoryPath = ".\\logs";
	auto now = std::chrono::system_clock::now();
	//auto period = StringUtil::ToInt32(StringUtil::ReadString("log", "period"));

	//std::chrono::system_clock::duration preservationPeriod = std::chrono::hours(24 * period);

	//try
	//{
	//	for (const auto& file : std::filesystem::directory_iterator(directoryPath))
	//	{
	//		auto lastModifiedTime = std::filesystem::last_write_time(file);
	//		auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(lastModifiedTime);

	//		auto difference = now - systemTime;

	//		// 보존 기간보다 더 오래된 파일인지 확인
	//		if (difference > preservationPeriod)
	//		{
	//			std::filesystem::remove(file.path());
	//		}
	//	}
	//}
	/*catch (const std::exception& e)
	{
		LOG_INFO("old log delete exception : %", e.what());
	}*/
}

