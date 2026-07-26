#ifndef STATUS_HPP
# define STATUS_HPP

# include <string>

// Map an HTTP status code to its reason phrase.
std::string get_status_string(size_t status);

#endif
