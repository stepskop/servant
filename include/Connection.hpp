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

/*
 * One client connection: its socket, the request being read, the response
 * being written, and any CGI child in between.
 */
class Connection {
    public:
        ~Connection();
        Connection(int fd, const std::vector<const ServerConfig*>* server_group);

        // Client socket fd.
        int fd;
        // Default server for the listener this connection arrived on.
        const ServerConfig *server;
        // Location matched for the current request.
        const LocationConfig *location;
        // Servers bound to the arrival listener, for per-request refinement.
        const std::vector<const ServerConfig*> *server_group;
        // Where the connection is in the read/serve/write cycle.
        ConnectionState state;
        // Bytes received but not yet framed into a request.
        std::string in_buf;
        // Serialized response bytes pending write.
        std::string out_buf;
        // Bytes of out_buf already written.
        size_t sent;
        // The request currently being read or served.
        Request req;
        // Status of the response being sent.
        size_t res_status;
        // Active CGI child, or NULL when none.
        CgiProcess *cgi;
        // Whether the connection stays open after this response.
        bool keep_alive;
        // Timestamp of the last I/O, for timeout enforcement.
        time_t last_activity;

        // Queue a response for writing.
        void send(Response res);
        // Send an error response and close the connection.
        void fail(Response res);
        // Take newly received bytes and advance request framing.
        bool consume(const char* data, size_t len);
        // Advance request framing over already-buffered bytes.
        bool frame();
        // Whether this request needs a CGI child registered with the loop.
        bool should_register_cgi() const;
        // Stop and clean up the CGI child.
        void teardown_cgi();

        // Reset per-request state to read the next request on a kept-alive conn.
        void reset();
    private:
        Connection(const Connection& src);
        Connection& operator=(const Connection& src);
};

#endif
