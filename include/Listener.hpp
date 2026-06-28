#ifndef LISTENER_HPP
# define LISTENER_HPP

#include <stdint.h>
#include <string>

class Listener {
    public:
        int fd;

        Listener(const std::string& host, uint16_t port);
        ~Listener();
        int start();
    private:
        std::string host;
        uint16_t port;
};

#endif
