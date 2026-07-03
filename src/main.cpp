#include <cstddef>
#include <signal.h>
#include <iostream>
#include <utility>
#include <vector>
#include "Banner.hpp"
#include "Config.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

int main(int argc, char** argv) {
    if (argc > 2) {
        Logger::error("Usage: ./webserv [config file]");
        return 1;
    }

    Config config;
    std::string config_path = "./default.conf";
    if (argc == 2) {
        config_path = argv[1];
    }

    if (load_config(config_path, config) == -1) {
        Logger::error(std::string("Failed to load config: ") + config_path);
        return 1;
    }

    std::cout << SERVANT_BANNER << std::endl;

    if (argc != 2) {
        Logger::warn("Using default config");
    }

    // Ignore SIGPIPE. Piping to closed FD should not kill the server.
    signal(SIGPIPE, SIG_IGN);

    // Map keyed by "host:port", value is vector of servers - first is default (added as listener), rest are "virtual hosts".
    typedef std::map<std::string, std::vector<ServerConfig*> > ServerMap;

    ServerMap servers;
    for (size_t i = 0; i < config.servers.size(); i++) {
        ServerConfig &configured_server = config.servers[i];
        std::string key = Str() << configured_server.host << ":" << configured_server.port; // TODO: There can be conflict as 0.0.0.0:80 and 127.0.0.1:80.

        servers[key].push_back(&configured_server);
    }

    EventLoop loop;
    // Register listeners for default host of each server.
    for (ServerMap::iterator it = servers.begin(); it != servers.end(); ++it) {
        loop.add_listener(it->second[0]);
    }

    return loop.run();
}
