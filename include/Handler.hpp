#ifndef STATIC_FILE_HANDLER_HPP
# define STATIC_FILE_HANDLER_HPP

# include "Connection.hpp"

// Resolve the request against the server root and serve a static file.
void serve_static(Connection& conn);

// Store an uploaded request body under the location's upload directory.
void upload_file(Connection& conn);

// Delete the file the request targets.
void delete_file(Connection& conn);

#endif
