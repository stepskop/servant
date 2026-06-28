#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <cstddef>
# include <string>
# include "Request.hpp"

# define MAX_HEADER_SIZE 8192 // 8 KB
# define MAX_BODY_SIZE 10240000 // 10 MB

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
        Connection(int fd);
        int fd;
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
