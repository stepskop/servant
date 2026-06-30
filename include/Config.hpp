#ifndef CONFIG_HPP
# define CONFIG_HPP

# include <string>
# include <vector>
# include <map>
# include <set>
# include <utility>
# include <cstddef>

enum ConfigTokenType {
  WORD,
  BLOCK_START,
  BLOCK_END,
  TERMINATOR,
};

typedef struct {
    ConfigTokenType type;
    std::string value;
    size_t line;
} ConfigToken;

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

// Read cursor over a token stream. Centralizes the bounds check so the parser
// never indexes past the end: every read goes through peek/advance/expect.
// The expect_* methods throw std::runtime_error on a type mismatch or EOF.
class Cursor {
    const std::vector<ConfigToken> &tokens;
    size_t                          pos;

public:
    explicit Cursor(const std::vector<ConfigToken> &tokens);

    bool                at_end() const;
    const ConfigToken  &peek() const;                  // guard with !at_end()
    bool                is(ConfigTokenType type) const;
    bool                is_word(const char *keyword) const;

    const ConfigToken  &advance();                     // guard with !at_end()
    size_t              expect(ConfigTokenType type);  // returns consumed line
    std::string         expect_word();
    void                expect_keyword(const char *keyword);
};

int load_config(const std::string &path, Config &config);

#endif
