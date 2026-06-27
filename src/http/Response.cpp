#include "Response.hpp"
#include "Status.hpp"
#include "Utils.hpp"
#include <string>
#include <sstream>

std::string build_response(size_t status, std::string body_str) {

    std::stringstream response;
    response << "HTTP/1.1 " << status << " " << get_status_string(status) << CRLF;
    response << "Connection: close" << CRLF;

    size_t body_size = body_str.size();
    response << "Content-Length: " << body_size << CRLF;
    response << CRLF;

    if (body_size != 0) {
        response << body_str;
    }

    return response.str();
}
