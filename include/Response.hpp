#ifndef RESPONSE_HPP
# define RESPONSE_HPP

#include <cstddef>
#include <string>
#include <map>

// A response under construction. Status + optional body + headers, built up
// via chained setters, then serialized to the wire form. Content-Type defaults
// to text/html; Connection: close and Content-Length are always emitted by
// serialize() and are not caller-settable.
class Response {
    public:
        Response(size_t status = 200);
        Response& body(const std::string& content);
        Response& header(const std::string& key, const std::string& value);
        size_t get_status() const;
        std::string serialize(bool exclude_body = false) const;
        bool has_body() const;
    private:
        size_t status;
        std::string body_str;
        std::map<std::string, std::string> headers;
};

#endif
