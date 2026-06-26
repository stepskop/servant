#include <arpa/inet.h>
#include <signal.h>
#include "../include/EventLoop.hpp"

int main() {
    // Ignore SIGPIPE. Piping to closed FD should not kill the server.
    signal(SIGPIPE, SIG_IGN);

    EventLoop loop;
    Listener *listener = new Listener(inet_addr("0.0.0.0"), 8080);

    loop.add_listener(listener);
    loop.run();

    delete listener;
    return 0;
}
