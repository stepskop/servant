#include "Config.hpp"
#include "ConfigParser.hpp"
#include "ConfigResolver.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <sstream>
#include <string>
#include <vector>

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
