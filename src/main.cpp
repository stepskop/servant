#include <signal.h>
#include <iostream>
#include "Banner.hpp"
#include "Config.hpp"
#include "EventLoop.hpp"

// TEMP: hardcoded config until the parser lands.
static Config default_config() {
    ServerConfig server;
    server.host = "0.0.0.0";
    server.port = "8080";
    server.root = "./www";
    server.index = "index.html";

    Config config;
    config.servers.push_back(server);
    return config;
}

int main() {
    std::cout << SERVANT_BANNER << std::endl;

    // Ignore SIGPIPE. Piping to closed FD should not kill the server.
    signal(SIGPIPE, SIG_IGN);

    // `config` must outlive the loop: listeners/connections hold ServerConfig*
    // into config.servers, which is stable as long as the vector isn't grown.
    Config config = default_config();

    EventLoop loop;

    for (size_t i = 0; i < config.servers.size(); i++) {
        loop.add_listener(&config.servers[i]);
    }

    return loop.run();
}
