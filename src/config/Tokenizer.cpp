#include "ConfigParser.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <cctype>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static ConfigToken create_token(ConfigTokenType type, size_t line_n, std::string value) {
    ConfigToken token;
    token.type = type;
    token.line = line_n;
    token.value = value;
    return token;
}

static bool is_word_char(char c) {
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
        while (pos + 1 < config_len && is_word_char(config_str[pos + 1])) {
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
    if (this->peek().type != type) {
        throw std::runtime_error(Str() << "line " << this->peek().line
            << ": expected " << token_type_name(type)
            << ", got " << token_type_name(this->peek().type));
    }
    return this->advance().line;
}

std::string Cursor::expect_word() {
    if (this->at_end()) throw std::runtime_error(at_eof_msg("a word"));
    if (this->peek().type != WORD) {
        throw std::runtime_error(Str() << "line " << this->peek().line << ": expected a word, got " << token_type_name(this->peek().type));
    }
    return this->advance().value;
}

void Cursor::expect_keyword(const char *keyword) {
    if (this->at_end()) {
        throw std::runtime_error(at_eof_msg(std::string("'") + keyword + "'"));
    }
    if (this->peek().type != WORD || this->peek().value != keyword) {
        throw std::runtime_error(Str() << "line " << this->peek().line << ": expected '" << keyword << "'");
    }
    this->advance();
}
