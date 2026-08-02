#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

# include <map>
# include "Connection.hpp"
# include "Listener.hpp"

/*
 * The poll-driven event loop: owns all listeners and connections and drives
 * every fd (client and CGI) through its readable/writable events.
 */
class EventLoop {
    private:
        // Active client connections, keyed by socket fd.
        std::map<int, Connection*> connections;
        // Listening sockets, keyed by fd.
        std::map<int, Listener*> listeners;
        // CGI pipe fds mapped back to their owning connection.
        std::map<int, Connection*> cgi_fds;
        // Streamed file-body fds mapped back to their owning connection.
        std::map<int, Connection*> file_fds;

        // Accept a new client on a listener and register it.
        void accept_connection(Listener*);
        // Close a connection and release its resources.
        void close_connection(Connection *);
        // Read available bytes from a client and advance its request.
        void handle_read(Connection *);
        // Flush buffered response bytes to a client.
        void handle_write(Connection *);
        // Read the next slice of a streamed file body into the write buffer.
        void file_read(Connection *);
        // Hand the framed request to the router for dispatch to a handler.
        void dispatch(Connection *);

        // Read CGI output from the child's stdout pipe.
        void cgi_read(Connection *);
        // Write the request body to the child's stdin pipe.
        void cgi_write(Connection *);
        // Stop watching the child's stdin once the body is fully sent.
        void cgi_stop_writing(Connection *);
        // Abandon a failed CGI exchange and answer an error.
        void cgi_fail(Connection *);
        // Turn completed CGI output into a response.
        void cgi_finish(Connection *);
        // Remove a connection's CGI fds from the poll set.
        void unregister_cgi(Connection *);

        // poll() timeout in ms until the nearest connection timeout; -1 when idle.
        int next_timeout_ms();
        // Enforce per-connection and CGI timeouts.
        void check_timeouts();
    public:
        ~EventLoop();
        // Create a listener for the given server group and register it.
        void add_listener(std::vector<const ServerConfig*> &servers);
        // Run the loop until shutdown. Returns a process exit code.
        int run();
};

#endif
