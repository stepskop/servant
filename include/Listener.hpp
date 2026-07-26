#ifndef LISTENER_HPP
# define LISTENER_HPP

#include <string>
# include "Config.hpp"

// A listening socket for one host:port, shared by the servers bound to it.
class Listener {
    public:
        // Listening socket fd.
        int fd;
        // Servers bound to this host:port; stamped onto accepted connections.
        std::vector<const ServerConfig*> &server_group;
        // Default server for this host:port.
        const ServerConfig* &server;

        Listener(std::vector<const ServerConfig*> &servers);
        ~Listener();

        // Open, bind and listen on the socket. Returns the fd, or -1 on failure.
        int start();
    private:
        std::string host;
        std::string port;
};

#endif
