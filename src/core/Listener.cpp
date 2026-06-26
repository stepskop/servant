#include "../../include/Listener.hpp"
#include "../../include/Logger.hpp"
#include "../../include/Utils.hpp"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

Listener::Listener(in_addr_t host, uint16_t port): fd(-1), port(port) {
    // Ensure the struct is empty.
    memset(&this->address, 0, sizeof(this->address));
    address.sin_addr.s_addr = host;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
}

Listener::~Listener() {
    if (this->fd != -1) close(this->fd);
}

int Listener::start() {
    // Open a socket.
    this->fd = socket(this->address.sin_family, SOCK_STREAM, 0);
    if (this->fd == -1) {
        Logger::error("Error creating socket.");
        return 1;
    }
    Logger::debug(Str() << "Server socket FD: " << this->fd);

    // Allow instant re-binding after restart;
    int one = 1;
    setsockopt(this->fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    // Set socket to non-blocking mode
    fcntl(this->fd, F_SETFL, O_NONBLOCK);


    int bind_res = bind(this->fd, (struct sockaddr *)&address, sizeof(address));
    if (bind_res == -1) {
        Logger::error("Unable to bind a socket.");
        return 1;
    }

    int listen_res = listen(this->fd, 5);
    if (listen_res == -1) {
        Logger::error("Error during listen on server socket.");
        return 1;
    }

    Logger::info(Str() << "Listening on " << inet_ntoa(this->address.sin_addr) << ":" << ntohs(this->address.sin_port));
    return 0;
}
