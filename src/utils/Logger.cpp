#include "utils/Logger.hpp"
#include "utils/Colors.hpp"

#include <iostream>
#include <ctime>

bool	Logger::_debugEnabled = false;

Logger::Logger() {}
Logger::~Logger() {}

void	Logger::setDebug(bool enabled)
{
	_debugEnabled = enabled;
}

std::string	Logger::_levelToColor(LogLevel level)
{
	switch (level)
	{
	case LOG_DEBUG:
		return RESET;
	case LOG_INFO:
		return BLUE;
	case LOG_WARN:
		return YELLOW;
	case LOG_ERROR:
		return RED;
	default:
		return RESET;
	}
}

std::string	Logger::_getTimestamp()
{
	std::time_t	now = std::time(NULL);
	std::tm*	timeInfo = std::localtime(&now);

	char	buffer[32];

	if (timeInfo == NULL)
		return "000-00-00 00:00:00";

	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);
	return (std::string(buffer));
}

std::string	Logger::_levelToString(LogLevel level)
{
	switch (level)
	{
		case LOG_DEBUG:
			return "DEBUG";
		case LOG_INFO:
			return "INFO";
		case LOG_WARN:
			return "WARN";
		case LOG_ERROR:
			return "ERROR";
		default:
			return "UNKNOWN";
	}
}

void	Logger::log(LogLevel level, const std::string& message)
{
	if (level == LOG_DEBUG && !_debugEnabled)
		return;
	std::ostream&	out = (level == LOG_ERROR) ? std::cerr : std::cout;
	out << _levelToColor(level) << "[" << _getTimestamp() << "] " << "[" << _levelToString(level)
		<< "] " << message << RESET << std::endl;
}

void	Logger::debug(const std::string& message)
{
	log(LOG_DEBUG, message);
}

void	Logger::info(const std::string& message)
{
	log(LOG_INFO, message);
}

void	Logger::warn(const std::string& message)
{
	log(LOG_WARN, message);
}

void	Logger::error(const std::string& message)
{
	log(LOG_ERROR, message);
}
