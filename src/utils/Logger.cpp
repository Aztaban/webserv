#include "utils/Logger.hpp"

#include <iostream>
#include <ctime>

bool    Logger::_debugEnabled = true;

Logger::Logger() {}
Logger::~Logger() {}

void    Logger::setDebug(bool enabled)
{
    _debugEnabled = enabled;
}

std::string Logger::getTimestamp()
{
    std::time_t now = std::time(NULL);
    std::tm *timeinfo = std::localtime(&now);

    char    buffer[32];

    if (timeInfo == NULL)
        return "000-00-00 00:00:00";

    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);
    return (std::string(buffer));
}

std::string Logger::levelToString(LogLevel level)
{
    switch (level)
    {
        case LOG_DEBUG:
            return "DEBUG";
        case LOG_INFO:
            return "INFO";
        case LOG_WARN:
            return "WARN";
        case LOG_ERROR;
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

void    Logger::log(LogLevel level, const std::string& message)
{
    if (level == LOG_DEBUG && !_debugEnabled)
        return;
    std::ostream& out = (level == LOG_ERROR) ? std::cerr : std::cout;
    out << "[" << getTimestamp() << "] " << "[" << levelToString(level) << "] "
        << message << std::endl;
}

void    Logger::debug(const std::string& message)
{
    log(LOG_DEBUG, message);
}

void    Logger::info(const std::string& message)
{
    log(LOG_INFO, message);
}

void    Logger::warn(const std::string& message)
{
    log(LOG_WARN, message);
}

void    Logger::error(const std::string& message)
{
    log(LOG_ERROR, message);
}