#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <cstddef>
# include <string>
# include "Request.hpp"
# include "Config.hpp"

# define MAX_HEADER_SIZE 8192 // 8 KB

enum ConnectionState {
  READING_HEADERS,
  READING_BODY,
  PROCESSING,
  WRITING,
  CLOSING,
};

class Connection {
    public:
        ~Connection();
        Connection(int fd, const ServerConfig* server);
        int fd;
        // Default server for the listener this connection arrived on. Phase 4
        // refines per-request via Host header; for now it drives root/index/
        // client_max_body_size.
        const ServerConfig* server;
        ConnectionState state;
        std::string in_buf;
        std::string out_buf;
        size_t sent;
        Request req;
        void respond(size_t status, const std::string& body = "", const std::string& content_type = "text/html");
        void redirect(const std::string& location);
        // Append freshly-received bytes and advance the request framing state machine.
        // When a full request is buffered returns true meaning that the request can be handled.
        bool consume(const char* data, size_t len);
    private:
        Connection(const Connection& src);
        Connection& operator=(const Connection& src);
};

#endif
