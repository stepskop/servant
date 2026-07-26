#include "Response.hpp"
#include "Status.hpp"
#include "Utils.hpp"
#include <sstream>

// Start a response with the given status; Content-Type defaults to text/html.
Response::Response(size_t status) : status(status) {
    this->headers["Content-Type"] = "text/html";
}

// Set the response body.
Response& Response::body(const std::string& content) {
    this->body_str = content;
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
    return !this->body_str.empty();
}

/*
 * Render the response to its HTTP/1.1 wire form. Supplies a default error page
 * when a 4xx+ carries no body, and drops the body for bodiless statuses and HEAD.
 */
std::string Response::serialize(bool exclude_body) const {
    std::string out_body = this->body_str;

    // Provide default error page when no body defined.
    if (this->status >= 400 && out_body.empty()) {
        out_body = Str() << "<p>Unlucko. I have only <strong>" << this->status << "</strong> :( </p>";
    }

    // 1xx, 204 No Content and 304 Not Modified are defined to carry no body and
    // no Content-Length (RFC 9110). Drop the body and both content headers.
    bool bodiless = this->status == 204 || this->status == 304 || (this->status >= 100 && this->status < 200);
    if (bodiless) out_body.clear();

    std::stringstream response;
    response << "HTTP/1.1 " << this->status << " " << get_status_string(this->status) << CRLF;
    if (!bodiless) response << "Content-Length: " << out_body.size() << CRLF;

    for (std::map<std::string, std::string>::const_iterator it = this->headers.begin(); it != this->headers.end(); ++it) {
        // Nothing to describe on a bodiless response -> skip Content-Type.
        if (bodiless && insensitive_equals(it->first, "Content-Type")) continue;
        response << it->first << ": " << it->second << CRLF;
    }
    response << CRLF;

    if (!exclude_body && !bodiless) {
        response << out_body;
    }

    return response.str();
}
