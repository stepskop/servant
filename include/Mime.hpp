#ifndef MIME_HPP
# define MIME_HPP

#include <string>

// Resolve a Content-Type from a file path's extension.
std::string get_mime_type(const std::string& path);

#endif
