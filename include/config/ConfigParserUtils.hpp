#ifndef CONFIGPARSERUTILS_HPP
# define CONFIGPARSERUTILS_HPP

# include <string>
# include <vector>
#include <cctype>
#include <sstream>

// Groups small shared helpers used by the config parser.
namespace ConfigParserUtils {

	std::string trim(const std::string& value);
	
	bool isNumber(const std::string& value);
	
	std::vector<std::string> splitTokens(const std::string& line);

}

#endif