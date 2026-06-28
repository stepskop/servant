#ifndef LISTENER_HPP
# define LISTENER_HPP

#include <string>

class Listener {
    public:
        int fd;

        Listener(const std::string& host, const std::string& port);
        ~Listener();
        int start();
    private:
        std::string host;
        std::string port;
};

#endif
