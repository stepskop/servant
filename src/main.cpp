#include <signal.h>
#include <iostream>
#include <vector>
#include "Banner.hpp"
#include "Config.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"

int main(int argc, char** argv) {
    Config config;

    std::string config_path = "./default.conf";
    if (argc == 2) {
        config_path = argv[1];
    }

    if (load_config(config_path, config) == -1) {
        Logger::error("Usage ./webserv <config file>");
        return 1;
    };

    std::cout << SERVANT_BANNER << std::endl;

    if (argc != 2) {
        Logger::warn("Using default config");
    };

    // Ignore SIGPIPE. Piping to closed FD should not kill the server.
    signal(SIGPIPE, SIG_IGN);

    EventLoop loop;

    for (size_t i = 0; i < config.servers.size(); i++) {
        loop.add_listener(&config.servers[i]);
    }

    return loop.run();
}
