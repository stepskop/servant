#include "Response.hpp"
#include "Status.hpp"
#include "Utils.hpp"
#include <sstream>

// Start a response with the given status; Content-Type defaults to text/html.
Response::Response(size_t status) : status(status), file_fd(-1), file_size(0) {
    this->headers["Content-Type"] = "text/html";
}

// Set the response body.
Response& Response::body(const std::string& content) {
    this->body_str = content;
    return *this;
}

/*
 * Serve the body straight from an open file: serialize() only announces its
 * length, the bytes are streamed later, a chunk at a time. The fd is passed
 * along, not owned -- Connection::send takes it over.
 */
Response& Response::file(int fd, size_t size) {
    this->file_fd = fd;
    this->file_size = size;
    return *this;
}

// Set or overwrite a header.
Response& Response::header(const std::string& key, const std::string& value) {
    this->headers[key] = value;
    return *this;
}

// The status code.
size_t Response::get_status() const {
    return this->status;
}

// Whether a non-empty body has been set.
bool Response::has_body() const {
    return !this->body_str.empty() || this->file_fd != -1;
}

// Fd the body is read from, or -1 when the body is in memory.
int Response::get_file_fd() const {
    return this->file_fd;
}

// Byte length of the file body.
size_t Response::get_file_size() const {
    return this->file_size;
}

/*
 * 1xx, 204 No Content and 304 Not Modified are defined to carry no body and no
 * Content-Length (RFC 9110).
 */
bool Response::is_bodiless() const {
    return this->status == 204 || this->status == 304 || (this->status >= 100 && this->status < 200);
}

/*
 * Render the response to its HTTP/1.1 wire form. Supplies a default error page
 * when a 4xx+ carries no body, and drops the body for bodiless statuses and HEAD.
 * A file body takes over the whole body: it contributes only its Content-Length
 * here, the bytes follow later, streamed from the fd.
 */
std::string Response::serialize(bool exclude_body) const {
    std::string out_body = this->body_str;

    // Provide default error page when no body defined.
    if (this->status >= 400 && out_body.empty() && this->file_fd == -1) {
        out_body = Str() << "<p>Unlucko. I have only <strong>" << this->status << "</strong> :( </p>";
    }

    // Drop the body and both content headers on a bodiless status.
    bool bodiless = this->is_bodiless();
    if (bodiless) out_body.clear();

    size_t content_length = this->file_fd != -1 ? this->file_size : out_body.size();

    std::stringstream response;
    response << "HTTP/1.1 " << this->status << " " << get_status_string(this->status) << CRLF;
    if (!bodiless) response << "Content-Length: " << content_length << CRLF;

    for (std::map<std::string, std::string>::const_iterator it = this->headers.begin(); it != this->headers.end(); ++it) {
        // Nothing to describe on a bodiless response -> skip Content-Type.
        if (bodiless && insensitive_equals(it->first, "Content-Type")) continue;
        response << it->first << ": " << it->second << CRLF;
    }
    response << CRLF;

    // A file body owns the whole body: it already set Content-Length above, so
    // writing an in-memory body here too would put more bytes on the wire than
    // were announced and desync the next response on a kept-alive connection.
    if (!exclude_body && !bodiless && this->file_fd == -1) {
        response << out_body;
    }

    return response.str();
}
