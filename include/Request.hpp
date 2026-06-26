#ifndef REQUEST_HPP
# define REQUEST_HPP

# include <map>
# include <string>

enum RequestMethod {
    GET
};

typedef struct {
    std::map<std::string, std::string> headers;
    RequestMethod method;
    std::string body;
    std::string target;
    std::string version;
} Request;

// Parse the header block (everything before the \r\n\r\n) into req.
// Returns the HTTP status: 200 on success, or 400 / 501 / 505 on a
// malformed or unsupported request.
int parse_request(const std::string& block, Request& req);

#endif
