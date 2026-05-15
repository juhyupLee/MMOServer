
static int32_t ReadInt(const char* label, int32_t defaultValue)
{
	std::cout << label << " [" << defaultValue << "]: ";
	std::string line;
	std::getline(std::cin, line);
	if (line.empty()) return defaultValue;
	try { return std::stoi(line); } catch (...) { return defaultValue; }
}

static BotConfig ReadConfigFromConsole()
{
	BotConfig cfg{ };

	std::cout << "\n=== DummyClient Bot Test ===\n";
	std::cout << "  1. Static send/recv\n";
	std::cout << "  2. Disconnect after sending\n";
	std::cout << "  3. Disconnect + reconnect (a fraction of bots)\n";
	int32_t s = ReadInt("Scenario (1-3)", 1);
	cfg.scenario = static_cast<BotScenario>(std::clamp(s, 1, 3));

	cfg.botCount         = ReadInt("Bot count",         10);
	cfg.packetsPerBot    = ReadInt("Packets per bot",   30);
	cfg.packetIntervalMs = ReadInt("Packet interval ms", 100);

	if (cfg.scenario == BotScenario::Reconnect)
	{
		cfg.reconnectRatePct  = ReadInt("Reconnect rate (%)",  50);
		cfg.reconnectDelayMs  = ReadInt("Reconnect delay ms",  1000);
	}

	std::cout << "\nStarting with: scenario=" << s
		<< " bots=" << cfg.botCount
		<< " pkt/bot=" << cfg.packetsPerBot
		<< " interval=" << cfg.packetIntervalMs << "ms";
	if (cfg.scenario == BotScenario::Reconnect)
	{
		std::cout << " reco-rate=" << cfg.reconnectRatePct << "%"
			<< " reco-delay=" << cfg.reconnectDelayMs << "ms";
	}
	std::cout << "\n\n";

	return cfg;
}

int main()
{
	auto cfg = ReadConfigFromConsole();

	DummyClient client(cfg);
	client.Initialize();
	client.Start();
	client.Run();
}
