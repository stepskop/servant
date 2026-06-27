#ifndef REQUEST_HPP
# define REQUEST_HPP

# include <map>
# include <string>

enum RequestMethod {
    GET
};

typedef struct {
    std::string method;     // raw token: "GET", "POST", ...
    std::string target;     // path only, e.g. "/index.html"
    std::string query;      // after first '?', no '?'; empty if none
    std::string version;    // "HTTP/1.1"
    std::map<std::string, std::string> headers; // lowercased names
    std::string body;
    size_t body_size;
} Request;

int parse_header(std::string &block, Request &req);

#endif
