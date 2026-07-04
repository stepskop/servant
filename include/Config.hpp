#ifndef CONFIG_HPP
# define CONFIG_HPP

# include <string>
# include <vector>
# include <map>
# include <set>
# include <utility>
# include <cstddef>

// Configuration model. Two layers: the *raw* model is the parser's
// verbatim output (strings, no defaults); the *typed* model is what the runtime
// reads, produced by resolve().

// Default body limit when `client_max_body_size` is omitted (1 MB).
# define DEFAULT_MAX_BODY_SIZE 1048576

struct RawLocationConfig {
    std::string                 path;
    std::vector<std::string>    methods;
    std::string                 root;
    std::string                 index;
    std::string                 autoindex;
    std::string                 redirect_code;
    std::string                 redirect_target;
    std::string                 client_max_body_size;
    std::string                 upload_dir;
    std::string                 cgi_extension;
    std::string                 cgi_interpreter;
    std::vector<std::pair<std::string, std::string> > error_pages; // (code, path)
};

struct RawServerConfig {
    std::string                    listen;
    std::set<std::string>          server_names;
    std::string                    root;
    std::string                    index;
    std::string                    client_max_body_size;
    std::vector<std::pair<std::string, std::string> > error_pages; // (code, path)
    std::vector<RawLocationConfig> locations;
};

struct RawConfig {
    std::vector<RawServerConfig> servers;
};

struct LocationConfig {
    std::string                 path;            // e.g. "/uploads"
    std::set<std::string>       methods;         // subset of {GET, POST, DELETE}
    std::string                 root;            // inherits server root if empty
    std::string                 index;           // inherits server index if empty
    bool                        autoindex;
    std::pair<int, std::string> redirect;    // status + target; status 0 == none
    std::string                 cgi_extension;   // e.g. ".py"
    std::string                 cgi_interpreter; // e.g. /usr/bin/python3
    std::string                 upload_dir;      // optional
    std::map<int, std::string>  error_pages; // status -> path
    std::size_t                 client_max_body_size;

    LocationConfig(): autoindex(false), redirect(0, "") {}
};

struct ServerConfig {
    std::string                 host;        // e.g. 0.0.0.0
    std::string                 port;        // kept as string for getaddrinfo
    std::set<std::string>       server_names;
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

int load_config(const std::string &path, Config &config);

#endif
