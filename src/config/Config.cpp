// Config defaults/validation live inline in the parser
// (ConfigParser::apply_defaults_and_validate). This translation unit is kept as
// a placeholder so the Makefile's CONFIG_SRC slot stays wired; move shared
// model helpers here if they grow beyond the parser.
#include "Config.hpp"
#include "Utils.hpp"
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
    std::ostringstream ss;

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

// --- Cursor -----------------------------------------------------------------
// Implements the read cursor declared in Config.hpp. The bounds check lives
// here once, so the parse_* functions below never index the vector blindly.

static std::string at_eof_msg(const std::string &expected) {
    return "unexpected end of config, expected " + expected;
}

Cursor::Cursor(const std::vector<ConfigToken> &tokens)
    : tokens(tokens), pos(0) {}

bool Cursor::at_end() const {
    return this->pos >= this->tokens.size();
}

const ConfigToken &Cursor::peek() const {
    return this->tokens[this->pos];
}

bool Cursor::is(ConfigTokenType type) const {
    return !this->at_end() && this->peek().type == type;
}

bool Cursor::is_word(const char *keyword) const {
    return this->is(WORD) && this->peek().value == keyword;
}

const ConfigToken &Cursor::advance() {
    return this->tokens[this->pos++];
}

size_t Cursor::expect(ConfigTokenType type) {
    if (this->at_end())
        throw std::runtime_error(at_eof_msg(token_type_name(type)));
    if (this->peek().type != type)
        throw std::runtime_error(Str() << "line " << this->peek().line
            << ": expected " << token_type_name(type)
            << ", got " << token_type_name(this->peek().type));
    return this->advance().line;
}

std::string Cursor::expect_word() {
    if (this->at_end())
        throw std::runtime_error(at_eof_msg("a word"));
    if (this->peek().type != WORD)
        throw std::runtime_error(Str() << "line " << this->peek().line
            << ": expected a word, got " << token_type_name(this->peek().type));
    return this->advance().value;
}

void Cursor::expect_keyword(const char *keyword) {
    if (this->at_end())
        throw std::runtime_error(at_eof_msg(std::string("'") + keyword + "'"));
    if (this->peek().type != WORD || this->peek().value != keyword)
        throw std::runtime_error(Str() << "line " << this->peek().line
            << ": expected '" << keyword << "'");
    this->advance();
}

// --- parser -----------------------------------------------------------------
// Each block function consumes its own closing '}'. parse_config catches the
// thrown std::runtime_error at the top.

// "directive value ;" — keyword already known to be the current WORD.
static std::string parse_single_value(Cursor &cursor) {
    cursor.advance();                       // consume the directive keyword
    std::string value = cursor.expect_word();
    cursor.expect(TERMINATOR);
    return value;
}

// location <path> { ... }
static LocationConfig parse_location(Cursor &cursor) {
    LocationConfig loc;

    cursor.expect_keyword("location");
    loc.path = cursor.expect_word();
    size_t brace_line = cursor.expect(BLOCK_START);

    while (true) {
        if (cursor.at_end())
            throw std::runtime_error(Str() << "unexpected EOF, location block opened on line " << brace_line);
        if (cursor.is(BLOCK_END)) { cursor.advance(); return loc; }
        if (!cursor.is(WORD))
            throw std::runtime_error(Str() << "line " << cursor.peek().line << ": expected a directive");

        const std::string keyword = cursor.peek().value;
        if (keyword == "methods") {
            cursor.advance();
            while (cursor.is(WORD))
                loc.methods.insert(cursor.advance().value);
            cursor.expect(TERMINATOR);
        } else if (keyword == "autoindex") {
            loc.autoindex = (parse_single_value(cursor) == "on");
        } else if (keyword == "return") {
            cursor.advance();
            std::string code = cursor.expect_word();
            std::string target = cursor.expect_word();
            std::istringstream(code) >> loc.redirect.first;
            loc.redirect.second = target;
            cursor.expect(TERMINATOR);
        } else if (keyword == "root") {
            loc.root = parse_single_value(cursor);
        } else if (keyword == "index") {
            loc.index = parse_single_value(cursor);
        } else {
            throw std::runtime_error(Str() << "line " << cursor.peek().line << ": unknown directive '" << keyword << "' in location block");
        }
    }
}

// server { ... }
static ServerConfig parse_server(Cursor &cursor) {
    ServerConfig server;

    cursor.expect_keyword("server");
    size_t brace_line = cursor.expect(BLOCK_START);

    while (true) {
        if (cursor.at_end())
            throw std::runtime_error(Str() << "unexpected EOF, server block opened on line " << brace_line);
        if (cursor.is(BLOCK_END)) { cursor.advance(); return server; }
        if (!cursor.is(WORD))
            throw std::runtime_error(Str() << "line " << cursor.peek().line << ": expected a directive");

        const std::string keyword = cursor.peek().value;
        if (keyword == "location") {
            server.locations.push_back(parse_location(cursor));
        } else if (keyword == "listen") {
            // TODO: Introduce more robust parsing.
            std::string addr = parse_single_value(cursor);
            std::vector<std::string> splitted = split(addr, ":");
            server.host = splitted[0];
            server.port = splitted[1];
        } else if (keyword == "root") {
            server.root = parse_single_value(cursor);
        } else if (keyword == "index") {
            server.index = parse_single_value(cursor);
        } else if (keyword == "client_max_body_size") {
            size_t *l = &server.client_max_body_size;
            safe_atol(parse_single_value(cursor), (long&)(l));
        } else {
            throw std::runtime_error(Str() << "line " << cursor.peek().line << ": unknown directive '" << keyword << "'");
        }
    }
}

// Top level: a sequence of `server { ... }` blocks.
static Config parse_config(const std::vector<ConfigToken> &tokens) {
    Config config;
    Cursor cursor(tokens);

    while (!cursor.at_end()) {
        if (!cursor.is_word("server"))
            throw std::runtime_error(Str() << "line " << cursor.peek().line << ": expected 'server' at top level");
        config.servers.push_back(parse_server(cursor));
    }
    return config;
}

int load_config(const std::string &path, Config &config) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) return -1;

    std::ostringstream buffer;
    buffer << file.rdbuf();

    std::vector<ConfigToken> tokens = tokenize(buffer.str());
    inspect_tokens(tokens);

    try {
        config = parse_config(tokens);
        std::ostringstream ss;
        ss << "parsed " << config.servers.size() << " server block(s)";
        Logger::debug(ss.str());
    } catch (const std::exception &e) {
        Logger::error(std::string("config: ") + e.what());
        return -1;
    }

    return 1;
}
