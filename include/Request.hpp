#ifndef REQUEST_HPP
# define REQUEST_HPP

# include <map>
# include <string>

// One parsed HTTP request, filled incrementally as bytes arrive.
class Request {
    public:
        Request();

        // Request method token.
        std::string method;
        // Request path, without the query string.
        std::string target;
        // Query string, without the leading '?'.
        std::string query;
        // HTTP version.
        std::string version;
        // Request headers, names lowercased.
        std::map<std::string, std::string> headers;
        // Decoded message body.
        std::string body;
        // Bytes of body received so far.
        size_t body_size;
        // Whether the request line and headers are parsed.
        bool initialized;
        // Whether the body uses chunked transfer-encoding.
        bool chunked;
};

// Parse the request line and headers into req.
int parse_header(const std::string &block, Request &req);

// Decode chunked body bytes from in_buf into req, enforcing the body limit.
int unchunk_data(std::string &in_buf, Request &req, size_t max_body);
#endif
