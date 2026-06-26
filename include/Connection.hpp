#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <cstddef>
# include <string>
# include "Request.hpp"

# define MAX_HEADER_SIZE 8192 // 8 KB;

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
    private:
        Connection(const Connection& src);
        Connection& operator=(const Connection& src);
};

#endif
