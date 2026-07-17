#ifndef REQUEST_HPP
# define REQUEST_HPP

# include <map>
# include <string>

class Request {
    public:
        Request();
        std::string method;     // raw token: "GET", "POST", ...
        std::string target;     // path only, e.g. "/index.html"
        std::string query;      // after first '?', no '?'; empty if none
        std::string version;    // "HTTP/1.1"
        std::map<std::string, std::string> headers; // lowercased names
        std::string body;
        size_t body_size;
        bool initialized;
        bool chunked;
};

int parse_header(const std::string &block, Request &req);
int unchunk_data(std::string &in_buf, Request &req, size_t max_body);
#endif
