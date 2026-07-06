#ifndef CGI_HPP
# define CGI_HPP

# include <sys/types.h>
# include <ctime>
# include <string>
# include "Response.hpp"

# define CGI_TIMEOUT 30 // Seconds a CGI child may run before it is killed.

class CgiProcess {
    public:
        pid_t pid;

        int stdin_fd;
        int stdout_fd;

        std::string in_buf;
        size_t in_sent;

        std::string out_buf;

        // Wall-clock start, used to enforce CGI_TIMEOUT.
        time_t started;

        CgiProcess(pid_t pid, int stdin_fd, int stdout_fd);
        ~CgiProcess();
    private:
        CgiProcess(const CgiProcess&);
        CgiProcess& operator=(const CgiProcess&);
};

class Connection;

void handle_cgi(Connection &);

// Turn raw CGI script output (headers + blank line + body) into a Response,
// honouring Status:/Content-Type:/Location: and dropping headers the server
// owns (Content-Length, Connection).
Response parse_cgi_output(const std::string &raw);

#endif
