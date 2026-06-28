#include <signal.h>
#include "EventLoop.hpp"

int main() {
    // Ignore SIGPIPE. Piping to closed FD should not kill the server.
    signal(SIGPIPE, SIG_IGN);

    EventLoop loop;
    loop.add_listener("0.0.0.0", 8080);

    return loop.run();
}
