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
        /*
         * Take the body from an open file rather than a memory buffer, so the
         * bytes never have to be held in RAM. The fd is not owned by the
         * Response -- whoever sends it takes it over. Chainable.
         */
        Response& file(int fd, size_t size);
        // Set a header. Chainable.
        Response& header(const std::string& key, const std::string& value);
        // The status code.
        size_t get_status() const;
        // Render the response to its wire form.
        std::string serialize(bool exclude_body = false) const;
        // Whether a body has been set.
        bool has_body() const;
        // Fd the body is read from, or -1 when the body is in memory.
        int get_file_fd() const;
        // Byte length of the file body.
        size_t get_file_size() const;
        // Whether this status is defined to carry no body at all.
        bool is_bodiless() const;
    private:
        size_t status;
        std::string body_str;
        int file_fd;
        size_t file_size;
        std::map<std::string, std::string> headers;
};

#endif
