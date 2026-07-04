#ifndef LISTENER_HPP
# define LISTENER_HPP

#include <string>
# include "Config.hpp"

class Listener {
    public:
        int fd;
        // Default server for this host:port. Stamped onto every connection
        // accepted here (Phase 4 refines per-request via the Host header).
        std::vector<const ServerConfig*> &server_group;
        const ServerConfig* &server;

        Listener(std::vector<const ServerConfig*> &servers);
        ~Listener();
        int start();
    private:
        std::string host;
        std::string port;
};

#endif
