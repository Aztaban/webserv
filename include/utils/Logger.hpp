#ifndef LOGGER_HPP
# define LOGGER_HPP

# include <string>
# include <sstream>

enum LogLevel
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

class Logger {

private:
    Logger();
    Logger(const Logger& other);
    Logger& operator=(const Logger& other);
    ~Logger();

    static bool         _debugEnabled;
    static std::string  getTimestamp();
    static std::string  levelToString(LogLevel level);
    static std::string  levelToColor(LogLevel level);

public:
    static void setDebug(bool enabled);
    static void log(LogLevel level, const std::string& message);
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);

};

template <typename T>
std::string toString(const T& value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

#endif