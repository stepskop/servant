#include "EventLoop.hpp"
#include "Connection.hpp"
#include "Request.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Mime.hpp"
#include <cstddef>
#include <stdint.h>
#include <string>
#include <map>
#include <sys/fcntl.h>
#include <utility>
#include <poll.h>
#include <vector>
#include <fcntl.h>
#include <sys/socket.h>
#include <csignal>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

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

        return this->close_connection(connection);
    }
    // Append new buffer;
    connection->in_buf.append(buffer, recv_res);
    std::string header_end = Str() << CRLF << CRLF;
    size_t pos = connection->in_buf.find(header_end);

    if (connection->state == READING_HEADERS) {
        if (pos == std::string::npos) { // If no header found.
            if (connection->in_buf.length() < MAX_HEADER_SIZE) return; // Can read more.
            // Early reject while client may still be sending ->
            // close() with unread data sends RST, client may lose this 400.
            // Fix: Use shutdown() after this, and recv() until depleted.
            return connection->respond(400); // Cannot read more.
        }

        std::string header_block = connection->in_buf.substr(0, pos);

        int parse_res = parse_header(header_block, connection->req);

        // If parsing doesn't end with 200. Return 400: Bad request.
        if (parse_res != 200) return connection->respond(parse_res);

        // TEMP: Reject chunked requests.
        std::string transfer_encoding_raw = get_value(connection->req.headers, "transfer-encoding");
        if (!transfer_encoding_raw.empty() && transfer_encoding_raw == "chunked") {
            return connection->respond(501);
        }

        std::string content_length_raw = get_value(connection->req.headers, "content-length");

        if (!content_length_raw.empty()) {
            long content_length = 0;
            if (!is_digits(content_length_raw) || !safe_atol(content_length_raw, content_length)) return connection->respond(400);
            if (content_length > MAX_BODY_SIZE) return connection->respond(413); // Body is too big.
            if (content_length != 0) {
                // Remove header from the buffer.
                connection->in_buf.erase(0, pos + header_end.size());
                connection->req.body_size = content_length;
                connection->state = READING_BODY;
            }
        }
    }

    if (connection->state == READING_BODY) {
        if (connection->in_buf.size() < connection->req.body_size) return; // Need to read more;
        connection->req.body.swap(connection->in_buf);

        // If body is bigger = there is more some tail (maybe a new request piped)
        if (connection->req.body.size() > connection->req.body_size) {
            // Copy the tail.
            connection->in_buf.assign(connection->req.body, connection->req.body_size, std::string::npos);
        }
        connection->req.body.resize(connection->req.body_size);
    }

    // TEMP: Reject requests that are not GET.
    if (connection->req.method != "GET") return connection->respond(501);

    // Collapse "." / ".." lexically and reject any target that escapes ROOT.
    std::string safe_target;
    if (!normalize_path(connection->req.target, safe_target)) {
        Logger::warn(with_fd(connection->fd, Str() << "Path traversal blocked: " << connection->req.target));
        return connection->respond(403);
    }

    std::string file_path = Str() << ROOT << safe_target;

    Logger::debug(Str() << "Stating the file: " << file_path);
    struct stat sb;
    int stat_res = stat(file_path.c_str(), &sb);
    if (stat_res == -1) {
        Logger::error(with_fd(connection->fd, Str() << "Couldn't stat() the file: " << file_path));
        return connection->respond(404);
    }

    int stat_mode = sb.st_mode & S_IFMT;
    if (stat_mode != S_IFREG && stat_mode != S_IFDIR) {
        Logger::warn(with_fd(connection->fd, Str() << file_path << " is not a file or dir. Stat mode: " << (sb.st_mode & S_IFMT)));
        return connection->respond(403);
    }

    // TEMP: Autoindex from wish -> If dir append default file.
    if (stat_mode == S_IFDIR) {
        // No trailing slash -> 301 to "/sub/" so relative URLs resolve right.
        if (safe_target[safe_target.size() - 1] != '/') {
            return connection->redirect(safe_target + "/");
        }
        file_path.append(DEFAULT_FILE);

        // Re-stat it.
        int stat_res = stat(file_path.c_str(), &sb);
        if (stat_res == -1) {
            Logger::error(with_fd(connection->fd, Str() << "Couldn't stat() the file: " << file_path));
            return connection->respond(404);
        }
    }

    Logger::debug(Str() << "Opening the file: " << file_path);
    int file_fd = open(file_path.c_str(), O_RDONLY); // TODO: Make this not leak.
    if (file_fd == -1) {
        Logger::error(with_fd(connection->fd, Str() << "Couldn't open() the file: " << file_path));
        return connection->respond(403);
    }

    std::string content;
    char read_buf[8192];
    off_t total = 0;
    while (total < sb.st_size) {
        ssize_t n = read(file_fd, read_buf, sizeof(read_buf));
        if (n < 0) {
            Logger::error(with_fd(connection->fd, Str() << with_fd(file_fd, "Reading file failed.")));
            close(file_fd);
            return connection->respond(500);
        }
        if (n == 0) break;
        content.append(read_buf, n);
        total += n;
    }

    close(file_fd);
    return connection->respond(200, content, get_mime_type(file_path));
}

void EventLoop::handle_write(Connection *connection) {
    int fd = connection->fd;

    Logger::debug(with_fd(connection->fd, "Sending data."));
    int send_res = send(fd, connection->out_buf.data() + connection->sent, connection->out_buf.size() - connection->sent, 0);
    if (send_res <= 0) {
        if (send_res == -1) Logger::error(with_fd(connection->fd, "Error during while writing to the socket."));
        if (send_res == 0) Logger::warn(with_fd(connection->fd, "No bytes were sent."));

        return this->close_connection(connection);;
    }
    connection->sent += send_res;

    if (connection->sent == connection->out_buf.size()) {
        // TODO: If keep-alive -> reset to READING_HEADERS
        return this->close_connection(connection);;
    }
}

int EventLoop::run() {
    // Catch SIGINT/SIGTERM so poll() returns with EINTR and the loop exits cleanly.
    // No SA_RESTART -> poll is interrupted instead of auto-restarted.
    signal(SIGINT, on_stop_signal);
    signal(SIGTERM, on_stop_signal);

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

            // Is listener FD -> accept new connections.
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
            if (polled.revents & (POLLERR|POLLNVAL)) {
                Logger::error(with_fd(polled.fd, "Socket error. Closing."));
                this->close_connection(conn);
            } else if (polled.revents & POLLIN) {
                handle_read(conn);
            } else if (polled.revents & POLLOUT) {
                handle_write(conn);
            } else if (polled.revents & POLLHUP) {
                Logger::debug(with_fd(polled.fd, "Peer hung up. Closing."));
                this->close_connection(conn);
            }
        }
    }

    Logger::info("Shutdown signal received. Exiting.");
    return 0;
}
