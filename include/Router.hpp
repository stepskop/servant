#ifndef ROUTER_HPP
# define ROUTER_HPP

class Connection;

// Pick the server and location that handle the connection's request.
void resolve(Connection&);

// Dispatch the resolved request to the matching handler.
void route(Connection&);

#endif
