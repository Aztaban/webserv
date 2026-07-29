#include "core/WebServer.hpp"
#include "config/ServerConfig.hpp"
#include "config/ConfigParser.hpp"
#include "utils/Logger.hpp"

#include <string>
#include <exception>

#include <signal.h>

int main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);

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
		ConfigParser parser(configPath);
		std::vector<ServerConfig> configs = parser.parse();

		Logger::debug("Loaded " + toString(configs.size()) + " server configuration(s)");

		WebServer server(configs);
		server.setup();
		server.run();
	}
	catch (const std::exception &e)
	{
		Logger::error(std::string("webserv: ") + e.what());
		return 1;
	}

	return 0;
}
