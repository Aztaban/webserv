#include "config/ConfigParser.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << std::endl;
        std::exit(1);
    }
}

static std::string writeConfigFile(const std::string& fileName, const std::string& content)
{
    std::ofstream output(fileName.c_str());

    require(output.is_open(), "config fixture should open for writing");
    output << content;
    output.close();
    return fileName;
}

static void test_valid_config(void)
{
    std::string path = writeConfigFile(
        "tests/bin/config_valid.conf",
        "server {\n"
        "    listen 127.0.0.1:8080;\n"
        "    root ./www/site1;\n"
        "    index index.html;\n"
        "    client_max_body_size 12345;\n"
        "    error_page 404 ./www/site1/errors/404.html;\n"
        "    location /upload {\n"
        "        methods GET POST;\n"
        "        root ./www/site1;\n"
        "        upload on;\n"
        "        upload_path ./www/site1/uploads;\n"
        "    }\n"
        "}\n"
    );

    ConfigParser parser(path);
    std::vector<ServerConfig> servers = parser.parse();

    require(servers.size() == 1, "parser should return one server");
    require(servers[0].getHost() == "127.0.0.1", "host should be parsed");
    require(servers[0].getPort() == 8080, "port should be parsed");
    require(servers[0].getRoot() == "./www/site1", "root should be parsed");
    require(servers[0].getIndex() == "index.html", "index should be parsed");
    require(servers[0].getClientMaxBodySize() == 12345, "body size should be parsed");
    require(servers[0].getErrorpages().count(404) == 1, "error page should be stored");
    require(servers[0].getLocations().size() == 1, "one location should be parsed");
    require(servers[0].getLocations()[0].path == "/upload", "location path should be parsed");
    require(servers[0].getLocations()[0].uploadEnabled == true, "upload should be enabled");
    require(servers[0].getLocations()[0].uploadPath == "./www/site1/uploads", "upload path should be parsed");
}

static void test_invalid_config_throws(void)
{
    std::string path = writeConfigFile(
        "tests/bin/config_invalid.conf",
        "server {\n"
        "    listen 127.0.0.1:8080;\n"
        "    location / {\n"
        "        methods GET;\n"
        "    }\n"
        "\n"
    );

    ConfigParser parser(path);
    bool threw = false;

    try
    {
        (void)parser.parse();
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    require(threw, "parser should throw on unclosed server block");
}

int main(void)
{
    test_valid_config();
    test_invalid_config_throws();

    std::cout << "[PASS] ConfigParser unit tests" << std::endl;
    return (0);
}