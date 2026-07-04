#include "Response.hpp"
#include "Status.hpp"
#include "Utils.hpp"
#include <sstream>

Response::Response(size_t status) : status(status) {
    this->headers["Content-Type"] = "text/html";
}

Response& Response::body(const std::string& content) {
    this->body_str = content;
    return *this;
}

Response& Response::header(const std::string& key, const std::string& value) {
    this->headers[key] = value;
    return *this;
}

size_t Response::get_status() const {
    return this->status;
}

std::string Response::serialize() const {
    std::string out_body = this->body_str;

    // Provide default error page when no body defined.
    if (this->status >= 400 && out_body.empty()) {
        out_body = Str() << "<p>Unlucko. I have only <strong>" << this->status << "</strong> :( </p>";
    }

    std::stringstream response;
    response << "HTTP/1.1 " << this->status << " " << get_status_string(this->status) << CRLF;
    response << "Connection: close" << CRLF;
    response << "Content-Length: " << out_body.size() << CRLF;

    for (std::map<std::string, std::string>::const_iterator it = this->headers.begin(); it != this->headers.end(); ++it) {
        response << it->first << ": " << it->second << CRLF;
    }
    response << CRLF;
    response << out_body;

    return response.str();
}
