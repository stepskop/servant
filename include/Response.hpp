#ifndef RESPONSE_HPP
# define RESPONSE_HPP

#include <string>

std::string build_response(size_t status, std::string body_str = "", std::string content_type = "text/html");
std::string build_redirect(const std::string& location);

#endif
