#ifndef LOGGER_HPP
# define LOGGER_HPP

# include <string>
# include <sstream>

enum LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
};

class Logger {
    private:
        LogLevel level;
        void log_msg(std::string msg, LogLevel level);
    public:
        Logger(LogLevel level = INFO);

        void debug(std::string msg);
        void info(std::string msg);
        void warn(std::string msg);
        void error(std::string msg);
};

class Str {
    std::ostringstream ss;
public:
    template <typename T>
    Str& operator<<(const T& v) { ss << v; return *this; }
    operator std::string() const { return ss.str(); }
};

#endif
