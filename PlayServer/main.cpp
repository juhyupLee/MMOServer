#include "PlayServer.h"
#include "../ServerCore/Dump/MemoryDump.h"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
volatile std::sig_atomic_t g_stopSignal = 0;

void HandleSignal(int)
{
	g_stopSignal = 1;
}

std::string ReadEnvironment(const char* name, std::string fallback = {})
{
	const auto* value = std::getenv(name);
	return value == nullptr ? std::move(fallback) : std::string(value);
}

std::int64_t ParseInteger(const std::string& value, const char* option)
{
	try
	{
		std::size_t parsed = 0;
		const auto result = std::stoll(value, &parsed);
		if (parsed != value.size())
		{
			throw std::invalid_argument("trailing characters");
		}
		return result;
	}
	catch (const std::exception&)
	{
		throw std::runtime_error(
			std::string("Invalid value for ") + option + ": " + value);
	}
}

PlayServerConfig ParseConfig(int argc, char* argv[])
{
	PlayServerConfig config;
	if (const auto value = ReadEnvironment("MMO_PORT"); !value.empty())
	{
		config.port = static_cast<std::int32_t>(ParseInteger(value, "MMO_PORT"));
	}
	if (const auto value = ReadEnvironment("MMO_IO_THREADS"); !value.empty())
	{
		config.ioThreads = static_cast<std::uint32_t>(
			ParseInteger(value, "MMO_IO_THREADS"));
	}
	config.metricsFile = ReadEnvironment("MMO_METRICS_FILE");
	config.runID = ReadEnvironment("MMO_RUN_ID", "local");

	for (int index = 1; index < argc; ++index)
	{
		const std::string option = argv[index];
		auto nextValue = [&]() -> std::string
		{
			if (++index >= argc)
			{
				throw std::runtime_error("Missing value for " + option);
			}
			return argv[index];
		};

		if (option == "--port")
		{
			config.port = static_cast<std::int32_t>(ParseInteger(nextValue(), "--port"));
		}
		else if (option == "--io-threads")
		{
			config.ioThreads = static_cast<std::uint32_t>(
				ParseInteger(nextValue(), "--io-threads"));
		}
		else if (option == "--max-sessions")
		{
			config.maxSessions = static_cast<std::size_t>(
				ParseInteger(nextValue(), "--max-sessions"));
		}
		else if (option == "--metrics-interval-sec")
		{
			config.metricsIntervalSec = static_cast<std::int32_t>(
				ParseInteger(nextValue(), "--metrics-interval-sec"));
		}
		else if (option == "--metrics-file") config.metricsFile = nextValue();
		else if (option == "--run-id") config.runID = nextValue();
		else if (option == "--help")
		{
			std::cout
				<< "PlayServer options:\n"
				<< "  --port N --io-threads N --max-sessions N\n"
				<< "  --metrics-interval-sec N --metrics-file PATH --run-id ID\n";
			std::exit(0);
		}
		else
		{
			throw std::runtime_error("Unknown option: " + option);
		}
	}

	if (config.port <= 0 || config.port > 65535
		|| config.maxSessions == 0 || config.metricsIntervalSec <= 0)
	{
		throw std::runtime_error("PlayServer configuration is out of range");
	}
	return config;
}
}

int main(int argc, char* argv[])
{
	CrashDump crashDump;

	try
	{
		auto config = ParseConfig(argc, argv);
		config.externalStopSignal = &g_stopSignal;
		PlayServer server(std::move(config));
		std::signal(SIGINT, HandleSignal);
		std::signal(SIGTERM, HandleSignal);

		if (!server.Initialize() || !server.Start())
		{
			std::cerr << "PlayServer initialization failed\n";
			server.Release();
			return 1;
		}
		server.Run();
		server.Release();
		return 0;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "PlayServer fatal error: " << exception.what() << '\n';
		return 2;
	}
}
