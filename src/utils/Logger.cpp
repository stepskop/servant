#include "Logger.hpp"
#include "Utils.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>

static const char *RESET = "\033[0m";

/*
 * Threshold from the LOG_LEVEL environment variable, falling back to the
 * compile-time default when it is unset or holds something unrecognized.
 */
static LogLevel resolve_threshold() {
    const char *env = std::getenv("LOG_LEVEL");
    if (env == NULL) return LOG_LEVEL;

    std::string name = to_lower(env);
    if (name == "debug")               return DEBUG;
    if (name == "info")                return INFO;
    if (name == "warn" || name == "warning") return WARNING;
    if (name == "error")               return ERROR;
    return LOG_LEVEL;
}

// Whether to emit ANSI color. NO_COLOR set to any non-empty value turns it off.
static bool resolve_color() {
    const char *env = std::getenv("NO_COLOR");
    return env == NULL || env[0] == '\0';
}

// Prefix a message with its connection's fd tag, e.g. "[4] ...".
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

// ANSI color escape for a level.
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
    // Read the environment once, on the first message.
    static const LogLevel threshold = resolve_threshold();
    static const bool     color     = resolve_color();

    if (level < threshold) return;

    // ERROR to stderr, rest to stdout.
    std::ostream &out = (level >= ERROR) ? std::cerr : std::cout;

    out << "[" << (color ? color_for(level) : "") << label_for(level) << (color ? RESET : "") << "] "
        << msg
        << std::endl;
}
