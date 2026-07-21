#include "core/WebServer.hpp"
#include "config/ServerConfig.hpp"
#include "config/ConfigParser.hpp"
#include "utils/Logger.hpp"

#include <iostream>
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

		std::cout << "SERVERS: " << configs.size() << std::endl;

		for (std::size_t i = 0; i < configs.size(); i++)
		{
			std::cout
				<< "Server " << i
				<< " locations: "
				<< configs[i].getLocations().size()
				<< std::endl;
		}

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
