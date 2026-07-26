#include "Listener.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

// Prepare a listener for the group's first server host:port (socket not opened yet).
Listener::Listener(std::vector<const ServerConfig *> &server_group): fd(-1), server_group(server_group), server(server_group[0]) {
    this->host = server_group[0]->host;
    this->port = server_group[0]->port;
}

Listener::~Listener() {
    if (this->fd != -1) close(this->fd);
}

/*
 * Open, bind and listen on the host:port, trying each address getaddrinfo
 * returns until one binds. Returns 1 on success, 0 on failure.
 */
int Listener::start() {
    // Resolve host:port into a bindable address. getaddrinfo handles both numeric ("0.0.0.0") and named hosts.
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4 only.
    hints.ai_socktype = SOCK_STREAM; // TCP.
    hints.ai_flags = AI_PASSIVE;     // For binding a server socket.

    struct addrinfo *res = NULL;
    int gai = getaddrinfo(this->host.c_str(), this->port.c_str(), &hints, &res);
    if (gai) {
        Logger::error(Str() << "getaddrinfo(" << this->host << ":" << this->port << ") failed: " << gai_strerror(gai));
        return 0;
    }

    // A host may resolve to several candidates; keep the first we can bind.
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        this->fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (this->fd == -1) {
            Logger::warn(Str() << "socket() on " << this->host << ":" << this->port << " failed: " << strerror(errno));
            continue;
        }

        Logger::debug(with_fd(this->fd, "Opening server socket."));

        // Allow instant re-binding after restart.
        int one = 1;
        setsockopt(this->fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        // Set socket to non-blocking mode.
        set_nonblocking(this->fd);
        // Don't leak the listener into CGI children.
        set_cloexec(this->fd);

        if (bind(this->fd, p->ai_addr, p->ai_addrlen) == 0) break; // Bound.

        // Log the bind error before close() -- close() can overwrite errno.
        Logger::warn(Str() << "Binding " << this->host << ":" << this->port << " failed: " << strerror(errno));
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
