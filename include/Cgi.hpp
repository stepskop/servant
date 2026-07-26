#ifndef CGI_HPP
# define CGI_HPP

# include <sys/types.h>
# include <string>
# include "Response.hpp"

# define CGI_TIMEOUT 30 // Seconds a CGI child may run before it is killed.

// A running CGI child and the pipes the event loop pumps data through.
class CgiProcess {
    public:
        // Child process id.
        pid_t pid;

        // Pipe to the child's stdin (server writes the request body).
        int stdin_fd;
        // Pipe from the child's stdout (server reads the response).
        int stdout_fd;

        // Request body pending write to the child.
        std::string in_buf;
        // Bytes of in_buf already written.
        size_t in_sent;

        // Raw output accumulated from the child.
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

// Spawn the CGI child for the connection's request and wire up its pipes.
void handle_cgi(Connection &);

/*
 * Turn raw CGI script output (headers + blank line + body) into a Response,
 * honouring Status:/Content-Type:/Location: and dropping headers the server
 * owns (Content-Length, Connection).
 */
Response parse_cgi_output(const std::string &raw);

#endif
