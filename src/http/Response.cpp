#include "Response.hpp"
#include "Status.hpp"
#include "Utils.hpp"
#include <cstddef>
#include <string>
#include <sstream>

std::string build_response(size_t status, std::string body_str, std::string content_type) {

    std::stringstream response;
    response << "HTTP/1.1 " << status << " " << get_status_string(status) << CRLF;
    response << "Connection: close" << CRLF;

    // Provide default error page when no body defined.
    if (status >= 400 && body_str.empty()) {
        body_str = Str() << "<p>Unlucko. I have only <strong>" << status << "</strong> :( </p>";
    }

    size_t body_size = body_str.size();
    response << "Content-Length: " << body_size << CRLF;
    response << "Content-Type: " << content_type << CRLF;
    response << CRLF;

    if (body_size != 0) {
        response << body_str;
    }

    return response.str();
}

// 301 with a Location header — used to append the trailing slash on a
// directory request so relative URLs in the served page resolve correctly.
std::string build_redirect(size_t status, const std::string& location) {
    std::stringstream response;
    response << "HTTP/1.1 " << status << " " << get_status_string(status) << CRLF;
    response << "Connection: close" << CRLF;
    response << "Location: " << location << CRLF;
    response << "Content-Length: 0" << CRLF;
    response << CRLF;

    return response.str();
}
