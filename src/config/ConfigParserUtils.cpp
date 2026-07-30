#include "config/ConfigParserUtils.hpp"

#include <stdexcept>

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

// Parses a non-negative decimal value and rejects invalid input or overflow.
std::size_t parseUnsignedDecimal(
	const std::string& value,
	std::size_t maxValue,
	const std::string& fieldName)
{
	std::size_t	result = 0;

	if (value.empty())
		throw std::runtime_error(fieldName + " must not be empty");
	if (value[0] == '-')
		throw std::runtime_error(fieldName + " must not be negative");
	for (std::size_t i = 0; i < value.size(); i++)
	{
		if (!std::isdigit(static_cast<unsigned char>(value[i])))
			throw std::runtime_error(fieldName + " must contain only decimal digits");
		std::size_t	digit = static_cast<std::size_t>(value[i] - '0');
		if (result > maxValue / 10 || (result == maxValue / 10 && digit > maxValue % 10))
			throw std::runtime_error(fieldName + " is out of range");
		result = result * 10 + digit;
	}
	return result;
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