#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

# include <map>
# include "Connection.hpp"
# include "Listener.hpp"

class EventLoop {
    private:
        std::map<int, Connection*> connections;
        std::map<int, Listener*> listeners;
        void accept_connection(Listener*);
        void close_connection(Connection *);
        void handle_read(Connection *);
        void handle_write(Connection *);
    public:
        ~EventLoop();
        void add_listener(std::vector<const ServerConfig*> &servers);
        int run();
};

#endif
