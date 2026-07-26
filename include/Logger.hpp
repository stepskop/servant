#ifndef LOGGER_HPP
# define LOGGER_HPP

# include <string>

enum LogLevel { DEBUG, INFO, WARNING, ERROR };

#ifndef LOG_LEVEL
# define LOG_LEVEL INFO
#endif

// Static logger. Messages below LOG_LEVEL are dropped.
class Logger {
    private:
        // Shared sink: format and emit a message at the given level.
        static void log_msg(std::string msg, LogLevel level);
    public:
        static void debug(std::string msg);
        static void info(std::string msg);
        static void warn(std::string msg);
        static void error(std::string msg);
};

// Prefix a log message with its connection's fd for easier tracing.
std::string with_fd(int, std::string);

#endif
