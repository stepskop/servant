#ifndef CONFIG_HPP
# define CONFIG_HPP

# include <string>
# include <vector>
# include <map>
# include <set>
# include <utility>
# include <cstddef>

// Parsed, validated configuration model (spec B.3). Phase 3 populates the whole
// model but acts only on `listen` and the default server's
// root/index/client_max_body_size/error_pages. The rest is consumed by the
// Phase 4 router.

// Default body limit when `client_max_body_size` is omitted (1 MB).
# define DEFAULT_MAX_BODY_SIZE 1048576

struct LocationConfig {
    std::string             path;            // e.g. "/uploads"
    std::set<std::string>   methods;         // subset of {GET, POST, DELETE}
    std::string             root;            // inherits server root if empty
    std::string             index;           // inherits server index if empty
    bool                    autoindex;
    std::pair<int, std::string> redirect;    // status + target; status 0 == none
    std::string             cgi_extension;   // e.g. ".py"
    std::string             cgi_interpreter; // e.g. /usr/bin/python3
    std::string             upload_dir;      // optional

    LocationConfig(): autoindex(false), redirect(0, "") {}
};

struct ServerConfig {
    std::string                 host;        // e.g. 0.0.0.0
    std::string                 port;        // kept as string for getaddrinfo
    std::vector<std::string>    server_names;
    std::size_t                 client_max_body_size;
    std::string                 root;
    std::string                 index;
    std::map<int, std::string>  error_pages; // status -> path
    std::vector<LocationConfig> locations;

    ServerConfig():
        host("0.0.0.0"),
        client_max_body_size(DEFAULT_MAX_BODY_SIZE),
        index("index.html") {}
};

struct Config {
    std::vector<ServerConfig> servers;
};

#endif
