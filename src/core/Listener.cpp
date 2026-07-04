#include "Listener.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

Listener::Listener(std::vector<const ServerConfig *> &server_group): fd(-1), server_group(server_group), server(server_group[0]) {
    this->host = server_group[0]->host;
    this->port = server_group[0]->port;
}

Listener::~Listener() {
    if (this->fd != -1) close(this->fd);
}

int Listener::start() {
    // Resolve host:port into a bindable address. getaddrinfo handles both numeric ("0.0.0.0") and named hosts.
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4 only.
    hints.ai_socktype = SOCK_STREAM; // TCP.
    hints.ai_flags = AI_PASSIVE;     // For binding a server socket.

    struct addrinfo *res = NULL;
    if (getaddrinfo(this->host.c_str(), this->port.c_str(), &hints, &res)) {
        Logger::error("getaddrinfo failed.");
        return 0;
    }

    // A host may resolve to several candidates; keep the first we can bind.
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        this->fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (this->fd == -1) continue;

        Logger::debug(with_fd(this->fd, "Opening server socket."));

        // Allow instant re-binding after restart.
        int one = 1;
        setsockopt(this->fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        // Set socket to non-blocking mode.
        fcntl(this->fd, F_SETFL, O_NONBLOCK);

        if (bind(this->fd, p->ai_addr, p->ai_addrlen) == 0) break; // Bound.

        close(this->fd);
        this->fd = -1;
    }
    freeaddrinfo(res);

    if (this->fd == -1) {
        Logger::error(Str() << "Unable to create/bind a socket on " << this->host << ":" << this->port);
        return 0;
    }

    if (listen(this->fd, SOMAXCONN) == -1) {
        Logger::error("Error during listen on server socket.");
        return 0;
    }

    Logger::info(with_fd(this->fd, Str() << "Listening on " << this->host << ":" << this->port));
    return 1;
}
