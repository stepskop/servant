#ifndef LOGGER_HPP
# define LOGGER_HPP

# include <string>

enum LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
};

#ifndef LOG_LEVEL
# define LOG_LEVEL DEBUG
#endif

class Logger {
    private:
        static void log_msg(std::string msg, LogLevel level);
    public:
        static void debug(std::string msg);
        static void info(std::string msg);
        static void warn(std::string msg);
        static void error(std::string msg);
};

std::string with_fd(int, std::string);

#endif
