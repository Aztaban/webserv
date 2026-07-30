#include "config/ConfigParser.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
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


static std::string sizeToString(std::size_t value)
{
	std::ostringstream stream;

	stream << value;
	return stream.str();
}

static std::string writeConfigFile(const std::string& fileName, const std::string& content)
{
	std::ofstream output(fileName.c_str());

	require(output.is_open(), "config fixture should open for writing");
	output << content;
	output.close();
	return fileName;
}

static void requireParseFailure(
	const std::string& fileName,
	const std::string& content,
	const std::string& message)
{
	ConfigParser parser(writeConfigFile(fileName, content));
	bool threw = false;

	try
	{
		(void)parser.parse();
	}
	catch (const std::exception&)
	{
		threw = true;
	}
	require(threw, message);
}

static std::string getParseFailureMessage(
	const std::string& fileName,
	const std::string& content)
{
	ConfigParser parser(writeConfigFile(fileName, content));

	try
	{
		(void)parser.parse();
	}
	catch (const std::exception& error)
	{
		return error.what();
	}
	return std::string();
}

static void testValidConfigAndDirectiveOrder(void)
{
	std::string path = writeConfigFile(
		"tests/bin/config_valid.conf",
		"server {\n"
		"    root ./www/site1;\n"
		"    server_name example.test www.example.test;\n"
		"    index home.html;\n"
		"    client_max_body_size 12345;\n"
		"    error_page 404 ./www/site1/errors/404.html;\n"
		"    listen 127.0.0.1:8080;\n"
		"    location /upload/ {\n"
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
	require(servers[0].getRoot() == "./www/site1", "listen must not erase an earlier root");
	require(servers[0].getIndex() == "home.html", "listen must not erase an earlier index");
	require(servers[0].getClientMaxBodySize() == 12345, "listen must not erase the body limit");
	require(servers[0].getErrorpages().count(404) == 1, "listen must not erase error pages");
	require(servers[0].getServerNames().size() == 2, "multiple server names should be stored");
	require(servers[0].getServerNames()[0] == "example.test", "first server name should be stored");
	require(servers[0].getServerNames()[1] == "www.example.test", "second server name should be stored");
	require(servers[0].getLocations().size() == 1, "one location should be parsed");
	require(servers[0].getLocations()[0].path == "/upload", "location path should be normalized");
	require(servers[0].getLocations()[0].uploadEnabled, "upload should be enabled");
	require(servers[0].getLocations()[0].uploadPath == "./www/site1/uploads", "upload path should be parsed");
}

static void testServerNameInsideLocationIsRejected(void)
{
	requireParseFailure(
		"tests/bin/config_server_name_in_location.conf",
		"server {\n"
		"    location / {\n"
		"        server_name invalid.test;\n"
		"    }\n"
		"}\n",
		"server_name inside a location should be rejected");
}

static void testMissingSemicolonIsRejected(void)
{
	requireParseFailure(
		"tests/bin/config_missing_semicolon.conf",
		"server {\n"
		"    listen 8080\n"
		"}\n",
		"a directive without a semicolon should be rejected");
}

static void testUnexpectedTextAfterOpeningBraceIsRejected(void)
{
	requireParseFailure(
		"tests/bin/config_text_after_server_brace.conf",
		"server { unexpected\n"
		"}\n",
		"text after a server opening brace should be rejected");

	requireParseFailure(
		"tests/bin/config_text_after_location_brace.conf",
		"server {\n"
		"    location / { unexpected\n"
		"    }\n"
		"}\n",
		"text after a location opening brace should be rejected");
}

static void testUnexpectedTextAfterClosingBraceIsRejected(void)
{
	requireParseFailure(
		"tests/bin/config_text_after_closing_brace.conf",
		"server {\n"
		"} unexpected\n",
		"text after a closing brace should be rejected");
}

static void testDirectiveOutsideServerIsRejected(void)
{
	requireParseFailure(
		"tests/bin/config_directive_outside_server.conf",
		"listen 8080;\n"
		"server {\n"
		"}\n",
		"a directive outside a server block should be rejected");
}

static void testNestedServerIsRejected(void)
{
	requireParseFailure(
		"tests/bin/config_nested_server.conf",
		"server {\n"
		"    server {\n"
		"    }\n"
		"}\n",
		"nested server blocks should be rejected");
}

static void testNestedLocationIsRejected(void)
{
	requireParseFailure(
		"tests/bin/config_nested_location.conf",
		"server {\n"
		"    location / {\n"
		"        location /nested {\n"
		"        }\n"
		"    }\n"
		"}\n",
		"nested location blocks should be rejected");
}

static void testLocationOutsideServerIsRejected(void)
{
	requireParseFailure(
		"tests/bin/config_location_outside_server.conf",
		"location / {\n"
		"}\n",
		"a location outside a server block should be rejected");
}

static void testUnclosedBlockIsRejected(void)
{
	requireParseFailure(
		"tests/bin/config_unclosed.conf",
		"server {\n"
		"    location / {\n"
		"        methods GET;\n"
		"    }\n",
		"an unclosed server block should be rejected");
}


static void testListenPortValidation(void)
{
	std::string path = writeConfigFile(
		"tests/bin/config_port_max.conf",
		"server {\n"
		"    listen 65535;\n"
		"}\n");
	ConfigParser parser(path);
	std::vector<ServerConfig> servers = parser.parse();

	require(servers[0].getPort() == 65535, "port 65535 should be accepted");
	requireParseFailure(
		"tests/bin/config_port_empty.conf",
		"server {\n    listen 127.0.0.1:;\n}\n",
		"an empty listen port should be rejected");
	requireParseFailure(
		"tests/bin/config_port_text.conf",
		"server {\n    listen abc;\n}\n",
		"a non-numeric listen port should be rejected");
	requireParseFailure(
		"tests/bin/config_port_negative.conf",
		"server {\n    listen -1;\n}\n",
		"a negative listen port should be rejected");
	requireParseFailure(
		"tests/bin/config_port_zero.conf",
		"server {\n    listen 0;\n}\n",
		"port zero should be rejected");
	requireParseFailure(
		"tests/bin/config_port_too_large.conf",
		"server {\n    listen 65536;\n}\n",
		"ports above 65535 should be rejected");
	requireParseFailure(
		"tests/bin/config_port_overflow.conf",
		"server {\n    listen 999999999999999999999999999999999999;\n}\n",
		"overflowing listen ports should be rejected");
}

static void testBodySizeValidation(void)
{
	const std::size_t maximum = std::numeric_limits<std::size_t>::max();
	const std::string maximumText = sizeToString(maximum);
	std::string path = writeConfigFile(
		"tests/bin/config_body_size_max.conf",
		"server {\n"
		"    client_max_body_size " + maximumText + ";\n"
		"}\n");
	ConfigParser parser(path);
	std::vector<ServerConfig> servers = parser.parse();

	require(servers[0].getClientMaxBodySize() == maximum,
		"the maximum size_t body limit should be accepted");
	requireParseFailure(
		"tests/bin/config_body_size_empty.conf",
		"server {\n    client_max_body_size;\n}\n",
		"an empty body-size value should be rejected");
	requireParseFailure(
		"tests/bin/config_body_size_text.conf",
		"server {\n    client_max_body_size 12kb;\n}\n",
		"a non-numeric body-size value should be rejected");
	requireParseFailure(
		"tests/bin/config_body_size_negative.conf",
		"server {\n    client_max_body_size -1;\n}\n",
		"a negative body-size value should be rejected");
	requireParseFailure(
		"tests/bin/config_body_size_overflow.conf",
		"server {\n    client_max_body_size " + maximumText + "0;\n}\n",
		"a body-size value above size_t should be rejected");
}

static void testErrorPageStatusValidation(void)
{
	std::string path = writeConfigFile(
		"tests/bin/config_error_status_bounds.conf",
		"server {\n"
		"    error_page 400 ./400.html;\n"
		"    error_page 599 ./599.html;\n"
		"}\n");
	ConfigParser parser(path);
	std::vector<ServerConfig> servers = parser.parse();

	require(servers[0].getErrorpages().count(400) == 1,
		"error status 400 should be accepted");
	require(servers[0].getErrorpages().count(599) == 1,
		"error status 599 should be accepted");
	requireParseFailure(
		"tests/bin/config_error_status_low.conf",
		"server {\n    error_page 399 ./399.html;\n}\n",
		"error statuses below 400 should be rejected");
	requireParseFailure(
		"tests/bin/config_error_status_high.conf",
		"server {\n    error_page 600 ./600.html;\n}\n",
		"error statuses above 599 should be rejected");
	requireParseFailure(
		"tests/bin/config_error_status_negative.conf",
		"server {\n    error_page -404 ./404.html;\n}\n",
		"negative error statuses should be rejected");
	requireParseFailure(
		"tests/bin/config_error_status_text.conf",
		"server {\n    error_page nope ./error.html;\n}\n",
		"non-numeric error statuses should be rejected");
}

static void testRedirectStatusValidation(void)
{
	std::string path = writeConfigFile(
		"tests/bin/config_redirect_statuses.conf",
		"server {\n"
		"    location /old {\n"
		"        return 301 /new;\n"
		"    }\n"
		"    location /temporary {\n"
		"        return 302 /new;\n"
		"    }\n"
		"}\n");
	ConfigParser parser(path);
	std::vector<ServerConfig> servers = parser.parse();

	require(servers[0].getLocations()[0].redirectCode == 301,
		"redirect status 301 should be accepted");
	require(servers[0].getLocations()[1].redirectCode == 302,
		"redirect status 302 should be accepted");
	requireParseFailure(
		"tests/bin/config_redirect_status_low.conf",
		"server {\n    location / {\n        return 300 /new;\n    }\n}\n",
		"unsupported redirect status 300 should be rejected");
	requireParseFailure(
		"tests/bin/config_redirect_status_high.conf",
		"server {\n    location / {\n        return 303 /new;\n    }\n}\n",
		"unsupported redirect status 303 should be rejected");
	requireParseFailure(
		"tests/bin/config_redirect_status_negative.conf",
		"server {\n    location / {\n        return -301 /new;\n    }\n}\n",
		"negative redirect statuses should be rejected");
	requireParseFailure(
		"tests/bin/config_redirect_status_text.conf",
		"server {\n    location / {\n        return moved /new;\n    }\n}\n",
		"non-numeric redirect statuses should be rejected");
}


static void testMethodsDirectiveValidation(void)
{
	std::string path = writeConfigFile(
		"tests/bin/config_methods_valid.conf",
		"server {\n"
		"    location / {\n"
		"        methods GET POST DELETE;\n"
		"    }\n"
		"}\n");
	ConfigParser parser(path);
	std::vector<ServerConfig> servers = parser.parse();
	const std::vector<std::string>& methods = servers[0].getLocations()[0].methods;

	require(methods.size() == 3, "all supported methods should be accepted");
	require(methods[0] == "GET" && methods[1] == "POST" && methods[2] == "DELETE",
		"supported methods should preserve directive order");
	requireParseFailure(
		"tests/bin/config_methods_empty.conf",
		"server {\n    location / {\n        methods;\n    }\n}\n",
		"an empty methods directive should be rejected");
	requireParseFailure(
		"tests/bin/config_methods_duplicate.conf",
		"server {\n    location / {\n        methods GET POST GET;\n    }\n}\n",
		"duplicate methods should be rejected");
	requireParseFailure(
		"tests/bin/config_methods_unsupported.conf",
		"server {\n    location / {\n        methods GET PUT;\n    }\n}\n",
		"unsupported methods should be rejected");
}

static void testCgiExtensionHandoffValidation(void)
{
	std::string path = writeConfigFile(
		"tests/bin/config_cgi_mappings.conf",
		"server {\n"
		"    location /cgi {\n"
		"        cgi_extension .py /not/checked/python .php /not/checked/php;\n"
		"    }\n"
		"}\n");
	ConfigParser parser(path);
	std::vector<ServerConfig> servers = parser.parse();
	const LocationConfig& location = servers[0].getLocations()[0];

	require(location.cgiEnabled, "CGI metadata should be enabled when mappings exist");
	require(location.cgiExtensions.size() == 2,
		"all CGI extension/interpreter mappings should be stored");
	require(location.cgiExtensions.find(".py")->second == "/not/checked/python",
		"the CGI interpreter path should be stored without runtime validation");
	require(location.cgiExtensions.find(".php")->second == "/not/checked/php",
		"multiple CGI mappings should be preserved");
	requireParseFailure(
		"tests/bin/config_cgi_empty.conf",
		"server {\n    location /cgi {\n        cgi_extension;\n    }\n}\n",
		"cgi_extension without a mapping should be rejected");
	requireParseFailure(
		"tests/bin/config_cgi_incomplete_pair.conf",
		"server {\n    location /cgi {\n        cgi_extension .py;\n    }\n}\n",
		"cgi_extension with an incomplete pair should be rejected");
	requireParseFailure(
		"tests/bin/config_cgi_bad_extension.conf",
		"server {\n    location /cgi {\n        cgi_extension py /usr/bin/python3;\n    }\n}\n",
		"a CGI extension should begin with a dot");
}

static void testBooleanAndRequiredArgumentValidation(void)
{
	std::string path = writeConfigFile(
		"tests/bin/config_location_flags_valid.conf",
		"server {\n"
		"    location /one {\n"
		"        autoindex on;\n"
		"        upload off;\n"
		"    }\n"
		"    location /two {\n"
		"        autoindex off;\n"
		"        upload on;\n"
		"        upload_path ./uploads;\n"
		"        return 302 /target;\n"
		"    }\n"
		"}\n");
	ConfigParser parser(path);
	std::vector<ServerConfig> servers = parser.parse();

	require(servers[0].getLocations()[0].autoindex,
		"autoindex on should be accepted");
	require(!servers[0].getLocations()[0].uploadEnabled,
		"upload off should be accepted");
	require(!servers[0].getLocations()[1].autoindex,
		"autoindex off should be accepted");
	require(servers[0].getLocations()[1].uploadEnabled,
		"upload on should be accepted");
	requireParseFailure(
		"tests/bin/config_autoindex_invalid.conf",
		"server {\n    location / {\n        autoindex true;\n    }\n}\n",
		"autoindex should accept exactly on or off");
	requireParseFailure(
		"tests/bin/config_upload_invalid.conf",
		"server {\n    location / {\n        upload yes;\n    }\n}\n",
		"upload should accept exactly on or off");
	requireParseFailure(
		"tests/bin/config_upload_path_empty.conf",
		"server {\n    location / {\n        upload_path;\n    }\n}\n",
		"an empty upload_path should be rejected");
	requireParseFailure(
		"tests/bin/config_redirect_url_empty.conf",
		"server {\n    location / {\n        return 301;\n    }\n}\n",
		"an empty redirect URL should be rejected");
}

static void testUnknownDirectiveContextValidation(void)
{
	requireParseFailure(
		"tests/bin/config_unknown_server_directive.conf",
		"server {\n    autoindex on;\n}\n",
		"a location-only directive should be rejected at server level");
	requireParseFailure(
		"tests/bin/config_unknown_location_directive.conf",
		"server {\n    location / {\n        listen 8080;\n    }\n}\n",
		"a server-only directive should be rejected at location level");
	requireParseFailure(
		"tests/bin/config_unknown_directive.conf",
		"server {\n    nonsense value;\n}\n",
		"an unknown server directive should be rejected");
}


static void testDuplicateLocationPathsAreRejected(void)
{
	requireParseFailure(
		"tests/bin/config_duplicate_location.conf",
		"server {\n"
		"    location /files/ {\n"
		"    }\n"
		"    location /files {\n"
		"    }\n"
		"}\n",
		"normalized duplicate location paths should be rejected");
}

static void testDuplicateSingleValueDirectivesAreRejected(void)
{
	requireParseFailure(
		"tests/bin/config_duplicate_root.conf",
		"server {\n"
		"    root ./one;\n"
		"    root ./two;\n"
		"}\n",
		"duplicate server root directives should be rejected");
	requireParseFailure(
		"tests/bin/config_duplicate_listen.conf",
		"server {\n"
		"    listen 8000;\n"
		"    listen 8001;\n"
		"}\n",
		"duplicate listen directives should be rejected");
	requireParseFailure(
		"tests/bin/config_duplicate_autoindex.conf",
		"server {\n"
		"    location / {\n"
		"        autoindex on;\n"
		"        autoindex off;\n"
		"    }\n"
		"}\n",
		"duplicate location autoindex directives should be rejected");
	requireParseFailure(
		"tests/bin/config_duplicate_methods_directive.conf",
		"server {\n"
		"    location / {\n"
		"        methods GET;\n"
		"        methods POST;\n"
		"    }\n"
		"}\n",
		"duplicate methods directives should be rejected");
}

static void testRepeatableDirectiveKeysMustBeUnique(void)
{
	std::string path = writeConfigFile(
		"tests/bin/config_repeatable_directives.conf",
		"server {\n"
		"    server_name one.test two.test;\n"
		"    server_name three.test;\n"
		"    error_page 404 ./404.html;\n"
		"    error_page 500 ./500.html;\n"
		"    location /cgi {\n"
		"        cgi_extension .py /python;\n"
		"        cgi_extension .php /php;\n"
		"    }\n"
		"}\n");
	ConfigParser parser(path);
	std::vector<ServerConfig> servers = parser.parse();

	require(servers[0].getServerNames().size() == 3,
		"multiple unique server_name directives should remain additive");
	require(servers[0].getErrorpages().size() == 2,
		"different error_page status codes should remain additive");
	require(servers[0].getLocations()[0].cgiExtensions.size() == 2,
		"unique CGI mappings across directives should remain additive");
	requireParseFailure(
		"tests/bin/config_duplicate_server_name.conf",
		"server {\n"
		"    server_name Example.test;\n"
		"    server_name example.test;\n"
		"}\n",
		"duplicate server names should be rejected case-insensitively");
	requireParseFailure(
		"tests/bin/config_duplicate_error_page.conf",
		"server {\n"
		"    error_page 404 ./one.html;\n"
		"    error_page 404 ./two.html;\n"
		"}\n",
		"duplicate error_page status codes should be rejected");
	requireParseFailure(
		"tests/bin/config_duplicate_cgi_mapping.conf",
		"server {\n"
		"    location /cgi {\n"
		"        cgi_extension .py /one;\n"
		"        cgi_extension .py /two;\n"
		"    }\n"
		"}\n",
		"duplicate CGI extension mappings should be rejected");
}

static void testUploadConsistencyValidation(void)
{
	std::string path = writeConfigFile(
		"tests/bin/config_upload_order.conf",
		"server {\n"
		"    location /upload {\n"
		"        upload_path ./uploads;\n"
		"        upload on;\n"
		"    }\n"
		"}\n");
	ConfigParser parser(path);
	std::vector<ServerConfig> servers = parser.parse();

	require(servers[0].getLocations()[0].uploadEnabled,
		"upload consistency should be independent of directive order");
	requireParseFailure(
		"tests/bin/config_upload_without_path.conf",
		"server {\n"
		"    location /upload {\n"
		"        upload on;\n"
		"    }\n"
		"}\n",
		"upload on without upload_path should be rejected");
}

static void testParserErrorsIncludeLineAndDirective(void)
{
	std::string error = getParseFailureMessage(
		"tests/bin/config_error_context.conf",
		"server {\n"
		"    root ./one;\n"
		"    root ./two;\n"
		"}\n");

	require(!error.empty(), "invalid config should provide an error message");
	require(error.find(":3:") != std::string::npos,
		"parser errors should include the source line number");
	require(error.find("root") != std::string::npos,
		"parser errors should include the directive name where practical");
}

int main(void)
{
	testValidConfigAndDirectiveOrder();
	testServerNameInsideLocationIsRejected();
	testMissingSemicolonIsRejected();
	testUnexpectedTextAfterOpeningBraceIsRejected();
	testUnexpectedTextAfterClosingBraceIsRejected();
	testDirectiveOutsideServerIsRejected();
	testNestedServerIsRejected();
	testNestedLocationIsRejected();
	testLocationOutsideServerIsRejected();
	testUnclosedBlockIsRejected();
	testListenPortValidation();
	testBodySizeValidation();
	testErrorPageStatusValidation();
	testRedirectStatusValidation();
	testMethodsDirectiveValidation();
	testCgiExtensionHandoffValidation();
	testBooleanAndRequiredArgumentValidation();
	testUnknownDirectiveContextValidation();
	testDuplicateLocationPathsAreRejected();
	testDuplicateSingleValueDirectivesAreRejected();
	testRepeatableDirectiveKeysMustBeUnique();
	testUploadConsistencyValidation();
	testParserErrorsIncludeLineAndDirective();

	std::cout << "[PASS] ConfigParser unit tests" << std::endl;
	return 0;
}
