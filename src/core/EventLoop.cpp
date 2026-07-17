#include "EventLoop.hpp"
#include "Cgi.hpp"
#include "Connection.hpp"
#include "Logger.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "Router.hpp"
#include "Utils.hpp"
#include <cstddef>
#include <map>
#include <sys/poll.h>
#include <unistd.h>
#include <utility>
#include <poll.h>
#include <vector>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <csignal>
#include <cerrno>
#include <ctime>

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
        case WAITING_CGI:
        case PROCESSING: // Building a response
        case CLOSING: // FD to be closed
        default:
            return 0; // No need to wake up.
    }
}

// Seconds of allowed inactivity for a connection in the given state.
// Returns -1 for "no client-side timeout" (WAITING_CGI: the CgiProcess
// deadline owns that period; the client socket is parked by design).
static time_t timeout_for(const Connection &conn) {
    switch (conn.state) {
        case READING_HEADERS:
            // Empty buffer: keep-alive idle (or a fresh, silent connection).
            // Bytes buffered: a request is in flight — slowloris clock.
            return conn.in_buf.empty() ? IDLE_TIMEOUT : HEADER_TIMEOUT;
        case READING_BODY:
            return BODY_TIMEOUT;
        case WRITING:
            return WRITE_TIMEOUT;
        case WAITING_CGI:
            return -1;
        default:
            return IDLE_TIMEOUT;
    }
}

void EventLoop::add_listener(std::vector<const ServerConfig*> &server_group) {
    Listener *listener = new Listener(server_group);

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
    // Don't leak other clients' sockets into CGI children.
    set_cloexec(client_fd);

    // Store the new connection.
    std::pair<int, Connection*> new_connection = std::make_pair(client_fd, new Connection(client_fd, &from->server_group));
    this->connections.insert(new_connection);

    Logger::debug(with_fd(client_fd, "Connection accepted."));
}

void EventLoop::close_connection(Connection *connection) {
    Logger::debug(with_fd(connection->fd, "Closing the connection."));
    this->unregister_cgi(connection); // Drop cgi fds before ~Connection frees them (avoids dangling cgi_fds keys).
    this->connections.erase(connection->fd);
    delete connection;
}

void EventLoop::handle_read(Connection *connection) {
    char buffer[READ_BUFFER_SIZE];
    int fd = connection->fd;

    int recv_res = recv(fd, buffer, sizeof(buffer), 0);
    if (recv_res <= 0) {
        if (recv_res == -1) Logger::error(with_fd(fd, "Receiving failed."));
        if (recv_res == 0) Logger::info(with_fd(fd, "Connection closed by peer."));

        return this->close_connection(connection);
    }

    connection->last_activity = std::time(NULL);

    // Frame the request; serve only once a full request is buffered.
    bool can_serve = connection->consume(buffer, recv_res);
    if (!can_serve) return;

    route(*connection);

    if (connection->should_register_cgi()) {
        this->cgi_fds.insert(std::make_pair(connection->cgi->stdout_fd, connection));

        if (!connection->cgi->in_buf.empty()) {
            this->cgi_fds.insert(std::make_pair(connection->cgi->stdin_fd, connection));
        } else {
            // No body to feed -> close stdin now so the child sees EOF immediately
            // instead of blocking on a read that never gets data.
            close(connection->cgi->stdin_fd);
            connection->cgi->stdin_fd = -1;
        }
    }
}

void EventLoop::handle_write(Connection *connection) {
    int fd = connection->fd;

    if (connection->req.initialized) {
        // If the incoming request was successfuly parsed, we can log some details.
        Request req = connection->req;
        Logger::info(with_fd(connection->fd, Str() << req.method << " " << req.target << " " << req.version << " " << connection->res_status));
    } else {
        Logger::info(with_fd(connection->fd, Str() << "Malformed request " << connection->res_status ));
    }

    size_t to_send = connection->out_buf.size() - connection->sent;
    Logger::debug(with_fd(connection->fd, Str() << "Sending data. Size: " << to_send));
    int send_res = send(fd, connection->out_buf.data() + connection->sent, to_send, 0);
    if (send_res <= 0) {
        if (send_res == -1) Logger::error(with_fd(connection->fd, "Error during while writing to the socket."));
        if (send_res == 0) Logger::warn(with_fd(connection->fd, "No bytes were sent."));

        return this->close_connection(connection);
    }
    connection->sent += send_res;

    if (connection->sent == connection->out_buf.size()) {
        // TODO: If keep-alive -> reset to READING_HEADERS
        return this->close_connection(connection);
    }
}

void EventLoop::cgi_read(Connection *conn) {
    CgiProcess *cgi_process = conn->cgi;
    char buffer[READ_BUFFER_SIZE];

    int n = read(cgi_process->stdout_fd, buffer, sizeof(buffer));

    Logger::debug(with_fd(conn->fd, Str() << "Read " << n << " bytes from CGI process."));
    // read() failed -> fail the CGI process and close the connection.
    if (n < 0) return this->cgi_fail(conn);

    // More output -> accumulate and wait for the next readable event.
    if (n > 0) {
        cgi_process->out_buf.append(buffer, n);
        return;
    }

    // n == 0: child closed stdout (EOF). Output complete -> respond.
    this->cgi_finish(conn);
}

void EventLoop::cgi_write(Connection *conn) {
    CgiProcess *cgi_process = conn->cgi;
    size_t left = cgi_process->in_buf.size() - cgi_process->in_sent;
    int n = write(cgi_process->stdin_fd, cgi_process->in_buf.data() + cgi_process->in_sent, left);

    // A write error here is almost always EPIPE: the script exited (or stopped
    // reading) before draining the body.
    // That is not a server failure -- the script may have already printed a valid response.
    // Stop writing, close the pipe, and let stdout drive completion.
    if (n < 0) return this->cgi_stop_writing(conn);
    if (n == 0) return; // Nothing written -> wait for the next writable event.

    cgi_process->in_sent += n;

    // Body fully delivered
    if (cgi_process->in_sent == cgi_process->in_buf.size()) this->cgi_stop_writing(conn);
}

// Close the CGI stdin pipe (child sees EOF) and stop polling it for writes.
// Idempotent: safe whether the body was fully sent or a write hit EPIPE.
void EventLoop::cgi_stop_writing(Connection *conn) {
    CgiProcess *cgi_process = conn->cgi;
    if (cgi_process->stdin_fd == -1) return;
    close(cgi_process->stdin_fd);
    cgi_fds.erase(cgi_process->stdin_fd);
    cgi_process->stdin_fd = -1;
}

void EventLoop::unregister_cgi(Connection *conn) {
    if (!conn->cgi) return;
    cgi_fds.erase(conn->cgi->stdin_fd);
    cgi_fds.erase(conn->cgi->stdout_fd);
}

void EventLoop::cgi_finish(Connection *conn) {
    unregister_cgi(conn);

    // Ask "has the child exited?" without blocking the event loop. We reach here
    // on stdout EOF, so it has almost always already exited and WNOHANG returns
    // its exit code. If it hasn't exited yet (returns 0), we still have all the
    // output -- respond as success and LEAVE pid set so teardown_cgi() cleans up
    // the process. Clearing pid here would leave the dead child hanging around.
    int status = 0;
    bool ok = true;
    if (waitpid(conn->cgi->pid, &status, WNOHANG) == conn->cgi->pid) {
        conn->cgi->pid = -1; // already cleaned up -> ~CgiProcess must not touch it again
        ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    std::string raw = conn->cgi->out_buf;
    conn->teardown_cgi();

    // Nonzero exit / signal = upstream failure; otherwise parse the script's
    // headers + body (honouring its Status:/Content-Type:/Location:).
    conn->send(ok ? parse_cgi_output(raw) : Response(502));
}

void EventLoop::cgi_fail(Connection *conn) {
    this->unregister_cgi(conn);

    conn->teardown_cgi();
    conn->send(Response(502));
}

// Smallest ms until any connection state or CGI hits its timeout. -1 (block forever) when
// none are running; 0 when one is already past due so poll() returns at once.
int EventLoop::next_timeout_ms() {
    int timeout = -1;
    time_t now = std::time(NULL);

    for (std::map<int, Connection*>::iterator it = this->connections.begin(); it != this->connections.end(); it++) {
        const Connection *conn = it->second;
        const CgiProcess *cgi = conn->cgi;

        long s = 0;
        if (cgi) {
            s = (static_cast<long>(cgi->started) + CGI_TIMEOUT - now);
        } else {
            time_t t = timeout_for(*conn);
            if (t < 0) continue;   // Waiting for CGI (most likely)
            s = static_cast<long>(conn->last_activity) + t - now;
        }

        if (s < 0) s = 0; // already past due -> poll must return immediately

        long ms = s * 1000;
        if (timeout == -1 || ms < timeout) timeout = static_cast<int>(ms);
    }

    return timeout;
}

void EventLoop::check_timeouts() {
    time_t now = std::time(NULL);

    for (std::map<int, Connection*>::iterator it = this->connections.begin(); it != this->connections.end(); ) {
        Connection *conn = (it++)->second; // Advance now; close_connection below may erase conn.

        if (conn->cgi) {
            if (now - conn->cgi->started < CGI_TIMEOUT) continue;

            Logger::warn(with_fd(conn->fd, "CGI timed out. Killing."));

            this->unregister_cgi(conn);
            conn->teardown_cgi(); // SIGKILL + waitpid reaps the runaway child.

            conn->fail(Response(504));
        } else {
            time_t state_timeout = timeout_for(*conn);
            if (state_timeout < 0) continue; // Exempt (waiting for CGI) -> must match next_timeout_ms.
            if (now - conn->last_activity < state_timeout) continue;

            // If mid request.
            if (conn->state == READING_BODY || (conn->state == READING_HEADERS && !conn->in_buf.empty())) {
                conn->fail(Response(408));
            } else { // Stall WRITING / keep-alive with 0 bytes -> close silently, no request was made.
                this->close_connection(conn);
            }
        }
    }
}

int EventLoop::run() {
    if (this->listeners.size() <= 0) {
        Logger::error("No listeners to start with");
        return 1;
    }
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

        // Push CGI pollable FDs.
        for (std::map<int, Connection*>::iterator it = this->cgi_fds.begin(); it != this->cgi_fds.end(); it++) {
            struct pollfd cgi_pfd;
            cgi_pfd.fd = it->first;

            const Connection *conn = it->second;

            if (it->first == conn->cgi->stdin_fd) {
                cgi_pfd.events = POLLOUT;
            } else if (it->first == conn->cgi->stdout_fd) {
                cgi_pfd.events = POLLIN;
            } else {
                Logger::error(with_fd(it->first, "FD is registered in cgi_fds but is neither stdin nor stdout of the CGI process."));
                continue;
            }

            fds.push_back(cgi_pfd);
        }

        // Poll the FDs. Timeout is the nearest CGI deadline (-1 = block forever
        // when no CGI is running), so a hung child is reaped promptly.
        Logger::debug("Polling.");
        int ready_n = poll(&fds[0], fds.size(), this->next_timeout_ms());
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

            if (this->cgi_fds.count(polled.fd)) {
                Connection *conn = this->cgi_fds[polled.fd];
                if (polled.revents & (POLLIN|POLLHUP)) {
                    this->cgi_read(conn);
                } else if (polled.revents & POLLOUT) {
                    this->cgi_write(conn);
                } else if (polled.revents & (POLLERR|POLLNVAL)) {
                    this->cgi_fail(conn);
                }
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
                this->handle_read(conn);
            } else if (polled.revents & POLLOUT) {
                this->handle_write(conn);
            } else if (polled.revents & POLLHUP) {
                Logger::debug(with_fd(polled.fd, "Peer hung up. Closing."));
                this->close_connection(conn);
            }
        }

        // Stop any CGI past its deadline (covers poll() waking purely on timeout).
        this->check_timeouts();
    }

    Logger::info("Shutdown signal received. Exiting.");
    return 0;
}
