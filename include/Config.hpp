#ifndef CONFIG_HPP
# define CONFIG_HPP

# include <string>
# include <vector>
# include <map>
# include <set>
# include <utility>
# include <cstddef>

/*
 * Configuration model. Two layers: the *raw* model is the parser's verbatim
 * output (strings, no defaults); the *typed* model is what the runtime reads,
 * produced by resolve().
 */

// Default body limit when `client_max_body_size` is omitted (1 MB).
# define DEFAULT_MAX_BODY_SIZE 1048576

// Raw (unresolved) form of LocationConfig; see it for field meanings.
struct RawLocationConfig {
    std::string                 path;
    std::vector<std::string>    methods;
    std::string                 root;
    std::string                 alias;
    std::string                 index;
    std::string                 autoindex;
    std::string                 redirect_code;
    std::string                 redirect_target;
    std::string                 client_max_body_size;
    std::string                 upload_dir;
    std::string                 cgi_extension;
    std::string                 cgi_interpreter;
    std::vector<std::pair<std::string, std::string> > error_pages;
};

// Raw (unresolved) form of ServerConfig; see it for field meanings.
struct RawServerConfig {
    std::string                    listen;
    std::set<std::string>          server_names;
    std::string                    root;
    std::string                    index;
    std::string                    client_max_body_size;
    std::vector<std::pair<std::string, std::string> > error_pages;
    std::vector<RawLocationConfig> locations;
};

struct RawConfig {
    std::vector<RawServerConfig> servers;
};

struct LocationConfig {
    // URL path prefix this block matches, e.g. "/uploads".
    std::string                 path;
    // Allowed methods (subset of GET, POST, DELETE).
    std::set<std::string>       methods;
    // Filesystem root; inherits the server root if empty.
    std::string                 root;
    // If set, replaces the matched path prefix (nginx `alias`).
    std::string                 alias;
    // Index file; inherits the server index if empty.
    std::string                 index;
    // Whether to list directory contents.
    bool                        autoindex;
    // Redirect status and target; status 0 means none.
    std::pair<int, std::string> redirect;
    // File extension handled by CGI, e.g. ".py".
    std::string                 cgi_extension;
    // Interpreter for CGI scripts, e.g. /usr/bin/python3.
    std::string                 cgi_interpreter;
    // Directory uploads are written to; optional.
    std::string                 upload_dir;
    // Custom error pages, status -> path.
    std::map<int, std::string>  error_pages;
    // Max request body size, in bytes.
    std::size_t                 client_max_body_size;

    LocationConfig(): autoindex(false), redirect(0, "") {}

    // Map a request URL path to a filesystem path.
    std::string fs_path(const std::string &url_path) const;
};

struct ServerConfig {
    // Bind address, e.g. 0.0.0.0.
    std::string                 host;
    // Bind port, kept as a string for getaddrinfo.
    std::string                 port;
    // Names this server answers to (matched against the Host header).
    std::set<std::string>       server_names;
    // Max request body size, in bytes.
    std::size_t                 client_max_body_size;
    // Filesystem root for requests.
    std::string                 root;
    // Default index file.
    std::string                 index;
    // Custom error pages, status -> path.
    std::map<int, std::string>  error_pages;
    // Location blocks within this server.
    std::vector<LocationConfig> locations;

    ServerConfig():
        host("0.0.0.0"),
        client_max_body_size(DEFAULT_MAX_BODY_SIZE),
        index("index.html") {}
};

// The resolved configuration the runtime reads.
struct Config {
    std::vector<ServerConfig> servers;
};

// Load, parse and resolve the config file at path into config.
int load_config(const std::string &path, Config &config);

#endif
