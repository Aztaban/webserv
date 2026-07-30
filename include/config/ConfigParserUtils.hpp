#ifndef CONFIGPARSERUTILS_HPP
# define CONFIGPARSERUTILS_HPP

# include <cstddef>
# include <string>
# include <vector>
# include <cctype>
# include <sstream>

// Groups small shared helpers used by the config parser.
namespace ConfigParserUtils {

	std::string trim(const std::string& value);
	
	std::size_t parseUnsignedDecimal(
		const std::string& value,
		std::size_t maxValue,
		const std::string& fieldName);
	
	std::vector<std::string> splitTokens(const std::string& line);

}

#endif