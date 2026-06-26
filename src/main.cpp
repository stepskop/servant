#include <cstddef>
#include <stdint.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ostream>
#include <sstream>
#include <string>
#include <signal.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <poll.h>
#include <fcntl.h>
#include "../include/Logger.hpp"

std::string CRLF = "\r\n";

std::string with_fd(int fd, std::string msg) {
    std::stringstream ss;
    ss << "[" << fd << "]" << " " << msg;
    return ss.str();
}

int main() {
    Logger logger = Logger(DEBUG);

    // Ignore SIGPIPE. Piping to closed FD should not kill the server.
    signal(SIGPIPE, SIG_IGN);

    // Open a socket.
    int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd == -1) {
        logger.error("Error creating socket.");
        return 1;
    }
    logger.debug(Str() << "Server socket FD: " << server_socket_fd);

    // Allow instant re-binding after restart;
    int one = 1;
    setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    // Set socket to non-blocking mode
    fcntl(server_socket_fd, F_SETFL, O_NONBLOCK);

    sockaddr_in address;

    // Ensure the struct is empty.
    memset(&address, 0, sizeof(address));
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;

    uint16_t port = 8080;
    address.sin_port = htons(port);

    int bind_res = bind(server_socket_fd, (struct sockaddr *)&address, sizeof(address));
    if (bind_res == -1) {
        logger.error("Unable to bind a socket.");
        return 1;
    }

    int listen_res = listen(server_socket_fd, 5);
    if (listen_res == -1) {
        logger.error("Error during listen on server socket.");
        return 1;
    }

    logger.info(Str() << "Listening on " << port);

    std::vector<struct pollfd> fds;

    // TEMP: Seed one pollable FD
    struct pollfd server_socket_pfd;
    server_socket_pfd.fd = server_socket_fd;
    server_socket_pfd.events = POLLIN;

    fds.push_back(server_socket_pfd);

    // Event loop
    while (true) {

        logger.debug("Polling.");
        int ready_n = poll(&fds[0], fds.size(), -1); // -1 to make the poll timeout infinite
        logger.debug("Polled.");
        if (ready_n == -1) {
            logger.error("Polling failed.");
            return 1;
        }
        for (size_t i = fds.size(); i-- > 0;) { // Iterate backwards so we can erase() if needed.
            struct pollfd polled_fd = fds[i];

            // If FD is not ready, skip;
            if (!(polled_fd.revents & POLLIN)) continue;

            // If it is server FD, check for new connections.
            if (polled_fd.fd == server_socket_fd) {
                logger.debug(with_fd(server_socket_fd, "Checking for new connections."));
                int client_fd = accept(server_socket_fd, NULL, NULL);

                // If connection gets accepted.
                if (client_fd == -1) {
                    logger.error("Error during accept on of a new connection.");
                    continue;
                }

                // Set the new client connection to non-blocking mode.
                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                struct pollfd client_pfd;
                client_pfd.fd = client_fd;
                client_pfd.events = POLLIN;

                // Add it to the polling pool.
                fds.push_back(client_pfd);

                logger.debug(with_fd(client_fd, "Connection accepted."));
            } else { // Handle client FD
                char buffer[4096];
                int client_fd = polled_fd.fd;

                int recv_res = recv(client_fd, buffer, sizeof(buffer), 0);
                if (recv_res <= 0) {
                    if (recv_res == 0) logger.info(with_fd(client_fd, "Connection closed by peer."));
                    if (recv_res == -1) logger.error(with_fd(client_fd, "Receiving failed"));

                    // Close the FD and remove it from the polled pool.
                    close(client_fd);
                    fds.erase(fds.begin() + i);
                    continue;
                }

                std::stringstream body;
                body << "Mirek je borec" << std::endl;

                std::string body_str = body.str();

                std::stringstream response;
                response << "HTTP/1.1 200 OK" << CRLF;
                response << "Content-Length: " << body_str.size() << CRLF;
                response << "Connection: close" << CRLF;
                response << CRLF;

                response << body_str;

                std::string response_str = response.str();
                send(client_fd, response_str.c_str(), response_str.size(), 0);

                close(client_fd);
                fds.erase(fds.begin() + i);
                continue;
            }
        }
    }


    close(server_socket_fd);
    return 0;
}
