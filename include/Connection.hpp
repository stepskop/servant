#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <cstddef>
# include <string>
# include "Request.hpp"
# include "Config.hpp"
# include "Response.hpp"
# include "Cgi.hpp"

# define MAX_HEADER_SIZE 8192 // 8 KB
# define READ_BUFFER_SIZE 8192 // 8 KB

enum ConnectionState { READING_HEADERS, READING_BODY, PROCESSING, WAITING_CGI, WRITING, CLOSING };

class Connection {
    public:
        ~Connection();
        Connection(int fd, const std::vector<const ServerConfig*>* server_group);
        int fd;
        // Default server for the listener this connection arrived on. Phase 4
        // refines per-request via Host header; for now it drives root/index/
        // client_max_body_size.
        const ServerConfig *server;
        const LocationConfig *location;
        const std::vector<const ServerConfig*> *server_group;
        ConnectionState state;
        std::string in_buf;
        std::string out_buf;
        size_t sent;
        Request req;
        size_t res_status;
        CgiProcess *cgi;

        void send(Response res);
        // Append freshly-received bytes and advance the request framing state machine.
        // When a full request is buffered returns true meaning that the request can be handled.
        bool consume(const char* data, size_t len);
        bool should_register_cgi() const;
        // Tear down the CGI child: delete the process (dtor closes its fds and
        // reaps the child) and clear the pointer. Idempotent. The owning
        // EventLoop must unregister the fds from its poll set first.
        void teardown_cgi();
    private:
        Connection(const Connection& src);
        Connection& operator=(const Connection& src);
};

#endif
