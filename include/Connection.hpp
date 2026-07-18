#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <cstddef>
# include <string>
# include "Request.hpp"
# include "Config.hpp"
# include "Response.hpp"
# include "Cgi.hpp"

# define HEADER_TIMEOUT 10 // seconds
# define BODY_TIMEOUT   30
# define IDLE_TIMEOUT   15
# define WRITE_TIMEOUT  30

# define MAX_HEADER_SIZE 8192 // 8 KB
# define READ_BUFFER_SIZE 8192 // 8 KB

enum ConnectionState { READING_HEADERS, READING_BODY, WAITING_CGI, WRITING };

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
        bool keep_alive;
        time_t last_activity;

        void send(Response res);
        // Send an error response that abandons request framing: forces the
        // connection closed so leftover/unread bytes are never reparsed as a
        // new request. Use when framing is undecodable (400/501), the body was
        // left unread (408/413), or processing aborted (504).
        void fail(Response res);
        // Append freshly-received bytes, then advance framing. Returns true when
        // a full request is buffered and ready to serve.
        bool consume(const char* data, size_t len);
        // Advance the request framing state machine over in_buf without reading
        // new bytes. Re-entry point for a pipelined request already buffered
        // after the previous one was served (poll won't wake us for bytes we
        // already hold). Returns true when a full request is buffered.
        bool frame();
        bool should_register_cgi() const;
        // Tear down the CGI child: delete the process (dtor closes its fds and
        // reaps the child) and clear the pointer. Idempotent. The owning
        // EventLoop must unregister the fds from its poll set first.
        void teardown_cgi();

        void reset();
    private:
        Connection(const Connection& src);
        Connection& operator=(const Connection& src);
};

#endif
