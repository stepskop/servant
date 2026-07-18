#include "Config.hpp"
#include "ConfigParser.hpp"
#include "ConfigResolver.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <sstream>
#include <string>
#include <vector>

std::string LocationConfig::fs_path(const std::string &url_path) const {
    if (this->alias.empty()) {
        return this->root + url_path;
    }

    // Strip the matched location prefix (its trailing slash ignored), then
    // append whatever remains of the URL to the alias.
    std::string prefix = this->path;
    if (prefix.size() > 1 && prefix[prefix.size() - 1] == '/') {
        prefix.erase(prefix.size() - 1);
    }
    std::string rest = (url_path.compare(0, prefix.size(), prefix) == 0)
        ? url_path.substr(prefix.size())
        : url_path;
    return this->alias + rest;
}

int load_config(const std::string &path, Config &config) {
    std::string source;
    if (read_file(path, source) != 200) return -1;

    std::vector<ConfigToken> tokens = tokenize(source);
    inspect_tokens(tokens);

    try {
        RawConfig raw = parse_config(tokens);
        config = resolve(raw);

        std::ostringstream ss;
        ss << "Parsed " << config.servers.size() << " server block(s)";
        Logger::debug(ss.str());
    } catch (const std::exception &e) {
        Logger::error(std::string("config: ") + e.what());
        return -1;
    }

    return 1;
}
