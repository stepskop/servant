#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

# include <map>
# include "Connection.hpp"
# include "Listener.hpp"

class EventLoop {
    private:
        std::map<int, Connection*> connections;
        std::map<int, Listener*> listeners;
        std::map<int, Connection*> cgi_fds;
        void accept_connection(Listener*);
        void close_connection(Connection *);
        void handle_read(Connection *);
        void handle_write(Connection *);
        void cgi_read(Connection *);
        void cgi_write(Connection *);
        void cgi_stop_writing(Connection *);
        void cgi_fail(Connection *);
        void cgi_finish(Connection *);
        void unregister_cgi(Connection *);
        // poll() timeout (ms) until the nearest CGI deadline; -1 when no CGI is
        // running so the loop blocks instead of busy-waiting.
        int cgi_poll_timeout();
        // Kill + reap any CGI that has run past CGI_TIMEOUT, answering 504.
        void cleanup_cgi_timeouts();
    public:
        ~EventLoop();
        void add_listener(std::vector<const ServerConfig*> &servers);
        int run();
};

#endif
