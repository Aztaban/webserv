#include "core/WebServer.hpp"
#include "config/ServerConfig.hpp"

#include <iostream>
#include <string>
#include <exception>

int main(int argc, char **argv)
{
    if (argc > 2)
    {
        std::cerr << "Usage: ./webserv [configuration file]" << std::endl;
        return 1;
    }

    std::string configPath;

    if (argc == 2)
        configPath = argv[1];
    else
        configPath = "conf/default.conf";

    try
    {
        /*
            Temporary:
            We accept configPath so the program interface is correct,
            but we do not parse it yet.

            Later:
            ConfigParser parser(configPath);
            std::vector<ServerConfig> configs = parser.parse();
        */
        (void)configPath;

        ServerConfig config("127.0.0.1", 8000);

        WebServer server(config);
        server.setup();
        server.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "webserv: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}