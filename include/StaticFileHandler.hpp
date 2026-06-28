#ifndef STATIC_FILE_HANDLER_HPP
# define STATIC_FILE_HANDLER_HPP

# include "Connection.hpp"

# define ROOT "./www"
# define DEFAULT_FILE "index.html"

// Resolve conn.req against ROOT and serve a static file (or a directory's
// default file), writing the result via conn.respond() / conn.redirect().
void serve_static(Connection& conn);

#endif
