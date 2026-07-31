#include "core/WebServer.hpp"
#include "config/ServerConfig.hpp"
#include "config/ConfigParser.hpp"
#include "utils/Logger.hpp"
#include "utils/SignalState.hpp"

#include <string>
#include <exception>
#include <stdexcept>
#include <cstring>

#include <signal.h>

volatile sig_atomic_t	SignalState::shutdownRequested = 0;

void SignalState::requestShutdown(int signalNumber)
{
	(void)signalNumber;
	shutdownRequested = 1;
}

static void installSignalHandlers()
{
	struct sigaction	action;

	std::memset(&action, 0, sizeof(action));
	sigemptyset(&action.sa_mask);
	action.sa_handler = SignalState::requestShutdown;
	if (sigaction(SIGINT, &action, NULL) == -1 || sigaction(SIGTERM, &action, NULL) == -1)
		throw std::runtime_error("failed to install shutdown signal handlers");
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		throw std::runtime_error("failed to ignore SIGPIPE");
}

int main(int argc, char **argv)
{
	if (argc > 2)
	{
		Logger::error("Usage: ./webserv [configuration file]");
		return 1;
	}

	std::string configPath;

	if (argc == 2)
		configPath = argv[1];
	else
		configPath = "conf/default.conf";

	Logger::info("Starting webserv");
	Logger::debug("Config path: " + configPath);

	try
	{
		installSignalHandlers();

		ConfigParser parser(configPath);
		std::vector<ServerConfig> configs = parser.parse();

		Logger::debug("Loaded " + toString(configs.size()) + " server configuration(s)");

		WebServer server(configs);
		server.setup();
		server.run();
		Logger::info("Webserv stopped");
	}
	catch (const std::exception &e)
	{
		Logger::error(std::string("webserv: ") + e.what());
		return 1;
	}

	return 0;
}
