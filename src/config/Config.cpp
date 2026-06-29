// Config defaults/validation live inline in the parser
// (ConfigParser::apply_defaults_and_validate). This translation unit is kept as
// a placeholder so the Makefile's CONFIG_SRC slot stays wired; move shared
// model helpers here if they grow beyond the parser.
#include "Config.hpp"
#include "Logger.hpp"
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

ConfigToken create_token(ConfigTokenType type, size_t line_n, std::string value) {
    ConfigToken token;
    token.type = type;
    token.line = line_n;
    token.value = value;
    return token;
}

bool is_word(char c) {
    std::string disallowed = "{};#";
    if (std::isspace(static_cast<unsigned char>(c))) return false;

    for (size_t i = 0; i < disallowed.size(); i++) {
        if (c == disallowed[i]) return false;
    }

    return true;
}

std::vector<ConfigToken> tokenize(const std::string &config_str) {
    std::vector<ConfigToken> tokens;
    size_t line = 1;

    size_t config_len = config_str.size();
    for (size_t pos = 0; pos < config_len; pos++) {
        char c = config_str[pos];

        // Count lines on newline
        if (c == '\n') {
            line++;
            continue;
        }

        // Skip whitespace
        if (std::isspace(static_cast<unsigned char>(c))) continue;

        // Skip comments
        if (c == '#') {
            while (pos + 1 < config_len && config_str[pos + 1] != '\n') pos++;
            continue;
        }

        if (c == '{') {
            tokens.push_back(create_token(BLOCK_START, line, ""));
            continue;
        }
        if (c == '}') {
            tokens.push_back(create_token(BLOCK_END, line, ""));
            continue;
        }
        if (c == ';') {
            tokens.push_back(create_token(TERMINATOR, line, ""));
            continue;
        }

        // Get the word
        size_t start = pos;
        while (pos + 1 < config_len && is_word(config_str[pos + 1])) {
            pos++;
        }
        tokens.push_back(create_token(WORD, line, config_str.substr(start, pos - start + 1)));
    }

    return tokens;
}

static const char *token_type_name(ConfigTokenType type) {
    switch (type) {
        case WORD:        return "WORD";
        case BLOCK_START: return "BLOCK_START";
        case BLOCK_END:   return "BLOCK_END";
        case TERMINATOR:  return "TERMINATOR";
    }
    return "UNKNOWN";
}

void inspect_tokens(const std::vector<ConfigToken> &tokens) {
    std::stringstream ss;

    ss << tokens.size() << " token(s):\n";
    for (size_t i = 0; i < tokens.size(); i++) {
        const ConfigToken &t = tokens[i];
        ss << "  [" << i << "] line " << t.line
                  << "  " << token_type_name(t.type);
        if (!t.value.empty())
            ss << "  \"" << t.value << "\"";
        ss << "\n";
    }

    Logger::debug(ss.str());
}

int load_config(const std::string &path) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) return -1;

    std::ostringstream buffer;
    buffer << file.rdbuf();

    std::vector<ConfigToken> tokens = tokenize(buffer.str());

    inspect_tokens(tokens);

    return 1;
}
