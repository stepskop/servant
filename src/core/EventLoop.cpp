#include "../../include/EventLoop.hpp"
#include "../../include/Logger.hpp"
#include "../../include/Utils.hpp"
#include <cstddef>
#include <stdint.h>
#include <map>
#include <utility>
#include <sys/poll.h>
#include <vector>
#include <fcntl.h>
#include <sys/socket.h>
#include <csignal>
#include <cerrno>

// Set by the signal handler, polled by the run loop for a clean shutdown.
volatile sig_atomic_t g_stop = 0;

static void on_stop_signal(int) {
    g_stop = 1;
}

EventLoop::~EventLoop() {
    for (std::map<int, Connection*>::iterator it = this->connections.begin(); it != this->connections.end(); it++)
        delete it->second;
    for (std::map<int, Listener*>::iterator it = this->listeners.begin(); it != this->listeners.end(); it++)
        delete it->second;
}

short resolve_poll_event(ConnectionState state) {
    switch (state) {
        case READING_HEADERS:
        case READING_BODY:
            return POLLIN;
        case WRITING:
            return POLLOUT;
        case PROCESSING: // Building a response
        case CLOSING: // FD to be closed
        default:
            return 0; // No need to wake up.
    }
}

void EventLoop::add_listener(in_addr_t host, uint16_t port) {
    Listener *listener = new Listener(host, port);

    if (!listener->start()) {
        Logger::error("Listener didn't start");
        delete listener;
        return;
    }

    this->listeners.insert(std::make_pair(listener->fd, listener));
}

void EventLoop::accept_connection(Listener *from) {
    Logger::debug(with_fd(from->fd, "Checking for new connections."));
    int client_fd = accept(from->fd, NULL, NULL);

    // If connection gets accepted.
    if (client_fd == -1) {
        Logger::error("Error during accept on of a new connection.");
        return ;
    }

    // Set the new client connection to non-blocking mode.
    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    // Store the new connection.
    std::pair<int, Connection*> new_connection = std::make_pair(client_fd, new Connection(client_fd));
    this->connections.insert(new_connection);

    Logger::debug(with_fd(client_fd, "Connection accepted."));
}

void EventLoop::close_connection(Connection *connection) {
    Logger::debug(with_fd(connection->fd, "Closing the connection."));
    this->connections.erase(connection->fd);
    delete connection;
}

void EventLoop::handle_read(Connection *connection) {
    char buffer[4096];
    int fd = connection->fd;

    int recv_res = recv(fd, buffer, sizeof(buffer), 0);
    if (recv_res <= 0) {
        if (recv_res == -1) Logger::error(with_fd(fd, "Receiving failed."));
        if (recv_res == 0) Logger::info(with_fd(fd, "Connection closed by peer."));

        this->close_connection(connection);
        return;
    }
    // Append new buffer;
    connection->in_buf.append(buffer, recv_res);
    size_t pos = connection->in_buf.find(Str() << CRLF << CRLF);

    if (pos != std::string::npos) {
        // Parse request

        std::stringstream body;
        body << "Mirek je borec" << std::endl;
        std::string body_str = body.str();
        connection->out_buf.append(build_response(200, body_str));
        connection->sent = 0;
        connection->state = WRITING;
    } else if (connection->in_buf.length() >= MAX_HEADER_SIZE) {
        // If header not found;
        connection->out_buf.append(build_response(400));
        connection->sent = 0;
        connection->state = WRITING;
    } else {
        // Continue with current state (READING_HEADERS)
        return;
    }
}

void EventLoop::handle_write(Connection *connection) {
    int fd = connection->fd;

    Logger::debug(with_fd(connection->fd, "Sending data."));
    int send_res = send(fd, connection->out_buf.data() + connection->sent, connection->out_buf.size() - connection->sent, 0);
    if (send_res <= 0) {
        if (send_res == -1) Logger::error(with_fd(connection->fd, "Error during while writing to the socket."));
        if (send_res == 0) Logger::warn(with_fd(connection->fd, "No bytes were sent."));

        this->close_connection(connection);
        return ;
    }
    connection->sent += send_res;

    if (connection->sent == connection->out_buf.size()) {
        // TODO: If keep-alive -> reset to READING_HEADERS

        this->close_connection(connection);
        return;
    }
}

int EventLoop::run() {
    // Catch SIGINT/SIGTERM so poll() returns with EINTR and the loop exits cleanly.
    // No SA_RESTART -> poll is interrupted instead of auto-restarted.
    struct sigaction sa;
    sa.sa_handler = on_stop_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    while (!g_stop) {
        std::vector<struct pollfd> fds;

        // Push listeners pollable FDs.
        for (std::map<int, Listener*>::iterator it = this->listeners.begin(); it != this->listeners.end(); it++) {
            struct pollfd listener_pfd;
            listener_pfd.fd = it->first;
            listener_pfd.events = POLLIN;

            fds.push_back(listener_pfd);
        }

        // Push connections pollable FDs.
        for (std::map<int, Connection*>::iterator it = this->connections.begin(); it != this->connections.end(); it++) {
            struct pollfd connection_pfd;
            connection_pfd.fd = it->first;
            connection_pfd.events = resolve_poll_event(it->second->state);

            fds.push_back(connection_pfd);
        }

        // Poll the FDs.
        Logger::debug("Polling.");
        int ready_n = poll(&fds[0], fds.size(), -1); // -1 to make the poll timeout infinite
        Logger::debug("Polled.");
        if (ready_n == -1) {
            if (errno == EINTR) continue; // Signal arrived -> recheck g_stop.
            Logger::error("Polling failed.");
            continue; // Try polling again.
        }

        for (size_t i = 0; i < fds.size(); i++) {
            struct pollfd polled = fds[i];

            // Is listener FD -> accepts connections.
            if (this->listeners.count(polled.fd)) {
                if (polled.revents & POLLIN) this->accept_connection(this->listeners[polled.fd]);
                continue;
            }

            // No connection found with this FD;
            if (!connections.count(polled.fd)) {
                Logger::error(with_fd(polled.fd, "Expected this FD to belong to a connection. This should not happen."));
                continue;
            }
            Connection *conn = connections[polled.fd];

            // Else -> Is connection FD.
            if (polled.revents & (POLLHUP|POLLERR|POLLNVAL)) {
                Logger::debug(with_fd(polled.fd, "Closing errored connection."));
                this->close_connection(conn);
            } else if (polled.revents & POLLIN) {
                handle_read(conn);
            } else if (polled.revents & POLLOUT) {
                handle_write(conn);
            }
        }
    }

    Logger::info("Shutdown signal received. Exiting.");
    return 0;
}
