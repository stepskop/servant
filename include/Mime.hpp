#ifndef MIME_HPP
# define MIME_HPP

#include <string>

// Resolve a Content-Type from a file path's extension.
// Unknown / no extension -> "application/octet-stream".
std::string get_mime_type(const std::string& path);

#endif
