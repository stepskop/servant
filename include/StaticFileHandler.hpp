#ifndef STATIC_FILE_HANDLER_HPP
# define STATIC_FILE_HANDLER_HPP

# include "Connection.hpp"

// Resolve conn.req against the connection's server root and serve a static file.
void serve_static(Connection& conn);

#endif
