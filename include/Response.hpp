#ifndef RESPONSE_HPP
# define RESPONSE_HPP

#include <cstddef>
#include <string>
#include <map>

/*
 * An HTTP response, built up through chained setters and then serialized to
 * its wire form.
 */
class Response {
    public:
        Response(size_t status = 200);
        // Set the body. Chainable.
        Response& body(const std::string& content);
        // Set a header. Chainable.
        Response& header(const std::string& key, const std::string& value);
        // The status code.
        size_t get_status() const;
        // Render the response to its wire form.
        std::string serialize(bool exclude_body = false) const;
        // Whether a body has been set.
        bool has_body() const;
    private:
        size_t status;
        std::string body_str;
        std::map<std::string, std::string> headers;
};

#endif
