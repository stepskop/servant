#ifndef LISTENER_HPP
# define LISTENER_HPP

#include <stdint.h>
#include <arpa/inet.h>

class Listener {
    public:
        sockaddr_in address;
        int fd;
        uint16_t port;

        Listener(in_addr_t host, uint16_t port);
        ~Listener();
        int start();
};

#endif
