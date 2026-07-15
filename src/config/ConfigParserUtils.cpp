#include "config/ConfigParserUtils.hpp"

namespace ConfigParserUtils {

// Removes leading and trailing whitespace from one config line.
std::string trim(const std::string& value)
{
	std::size_t begin = 0;
	std::size_t end = value.size();

	while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])))
		begin++;
	while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
		end--;
	return value.substr(begin, end - begin);
}

// Checks whether a string contains only digits.
bool isNumber(const std::string& value)
{
	std::size_t i = 0;

	if (value.empty())
		return false;
	while (i < value.size())
	{
		if (!std::isdigit(static_cast<unsigned char>(value[i])))
			return false;
		i++;
	}
	return true;
}

// Splits one directive line into whitespace-separated tokens.
std::vector<std::string> splitTokens(const std::string& line)
{
	std::vector<std::string> tokens;
	std::istringstream stream(line);
	std::string token;

	while (stream >> token)
		tokens.push_back(token);
	return tokens;
}

}