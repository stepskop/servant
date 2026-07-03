#include "Config.hpp"
#include "ConfigParser.hpp"
#include "ConfigResolver.hpp"
#include "Logger.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

int load_config(const std::string &path, Config &config) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) return -1;

    std::ostringstream buffer;
    buffer << file.rdbuf();

    std::vector<ConfigToken> tokens = tokenize(buffer.str());
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
