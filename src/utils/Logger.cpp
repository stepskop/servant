#include "../../include/Logger.hpp"

#include <ctime>
#include <iostream>
#include <sstream>

static const char *RESET = "\033[0m";

std::string with_fd(int fd, std::string msg) {
    std::stringstream ss;
    ss << "[" << fd << "]" << " " << msg;
    return ss.str();
}

// label + color derived from level — single source of truth.
const char *label_for(LogLevel level) {
    switch (level) {
        case DEBUG:   return "DEBUG";
        case INFO:    return "INFO";
        case WARNING: return "WARN";
        case ERROR:   return "ERROR";
    }
    return "?";
}

const char *color_for(LogLevel level) {
    switch (level) {
        case DEBUG:   return "\033[36m"; // cyan
        case INFO:    return "\033[32m"; // green
        case WARNING: return "\033[33m"; // yellow
        case ERROR:   return "\033[31m"; // red
    }
    return RESET;
}

void Logger::debug(std::string msg) {
    log_msg(msg, DEBUG);
}

void Logger::info(std::string msg) {
    log_msg(msg, INFO);
}

void Logger::warn(std::string msg) {
    log_msg(msg, WARNING);
}

void Logger::error(std::string msg) {
    log_msg(msg, ERROR);
}

void Logger::log_msg(std::string msg, LogLevel level) {
    // Filter below compile-time threshold.
    if (level < LOG_LEVEL)
        return;

    // ERROR to stderr, rest to stdout.
    std::ostream &out = (level >= ERROR) ? std::cerr : std::cout;

    out << "[" << color_for(level) << label_for(level) << RESET << "] "
        << msg
        << std::endl;
}
