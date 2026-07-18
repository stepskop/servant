#include "ConfigParser.hpp"
#include "Utils.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

// "directive value ;" — keyword already known to be the current WORD.
static std::string parse_single_value(Cursor &cursor) {
    cursor.advance();                       // consume the directive keyword
    std::string value = cursor.expect_word();
    cursor.expect(TERMINATOR);
    return value;
}

// location <path> { ... }
static RawLocationConfig parse_location(Cursor &cursor) {
    RawLocationConfig loc;

    cursor.expect_keyword("location");
    loc.path = cursor.expect_word();
    size_t brace_line = cursor.expect(BLOCK_START);

    while (true) {
        if (cursor.at_end()) throw std::runtime_error(Str() << "unexpected EOF, location block opened on line " << brace_line);
        if (cursor.is(BLOCK_END)) { cursor.advance(); return loc; }
        if (!cursor.is(WORD)) throw std::runtime_error(Str() << "line " << cursor.peek().line << ": expected a directive");

        const std::string keyword = cursor.peek().value;
        if (keyword == "methods") {
            cursor.advance();
            while (cursor.is(WORD)) {
                loc.methods.push_back(cursor.advance().value);
            }
            cursor.expect(TERMINATOR);
        } else if (keyword == "autoindex") {
            loc.autoindex = parse_single_value(cursor);
        } else if (keyword == "return") {
            cursor.advance();
            loc.redirect_code = cursor.expect_word();
            loc.redirect_target = cursor.expect_word();
            cursor.expect(TERMINATOR);
        } else if (keyword == "root") {
            loc.root = parse_single_value(cursor);
        } else if (keyword == "alias") {
            loc.alias = parse_single_value(cursor);
        } else if (keyword == "index") {
            loc.index = parse_single_value(cursor);
        } else if (keyword == "client_max_body_size") {
            loc.client_max_body_size = parse_single_value(cursor);
        } else if (keyword == "upload_store") {
            loc.upload_dir = parse_single_value(cursor);
        } else if (keyword == "cgi") {
            cursor.advance();
            loc.cgi_extension = cursor.expect_word();
            loc.cgi_interpreter = cursor.expect_word();
            cursor.expect(TERMINATOR);
        } else if (keyword == "error_page") {
            cursor.advance();
            std::string code = cursor.expect_word();
            std::string page = cursor.expect_word();
            loc.error_pages.push_back(std::make_pair(code, page));
            cursor.expect(TERMINATOR);
        } else {
            throw std::runtime_error(Str() << "line " << cursor.peek().line << ": unknown directive '" << keyword << "' in location block");
        }
    }
}

// server { ... }
static RawServerConfig parse_server(Cursor &cursor) {
    RawServerConfig server;

    cursor.expect_keyword("server");
    size_t brace_line = cursor.expect(BLOCK_START);

    while (true) {
        if (cursor.at_end())
            throw std::runtime_error(Str() << "unexpected EOF, server block opened on line " << brace_line);
        if (cursor.is(BLOCK_END)) {
            cursor.advance();
            return server;
        }

        if (!cursor.is(WORD)) {
            throw std::runtime_error(Str() << "line " << cursor.peek().line << ": expected a directive");
        }

        const std::string keyword = cursor.peek().value;
        if (keyword == "location") {
            server.locations.push_back(parse_location(cursor));
        } else if (keyword == "listen") {
            server.listen = parse_single_value(cursor);   // "host:port" or "port"; split in resolve
        } else if (keyword == "server_name") {
            cursor.advance();
            while (cursor.is(WORD)) {
                server.server_names.insert(cursor.advance().value);
            }
            cursor.expect(TERMINATOR);
        } else if (keyword == "root") {
            server.root = parse_single_value(cursor);
        } else if (keyword == "index") {
            server.index = parse_single_value(cursor);
        } else if (keyword == "client_max_body_size") {
            server.client_max_body_size = parse_single_value(cursor);
        } else if (keyword == "error_page") {
            cursor.advance();
            std::string code = cursor.expect_word();
            std::string page = cursor.expect_word();
            server.error_pages.push_back(std::make_pair(code, page));
            cursor.expect(TERMINATOR);
        } else {
            throw std::runtime_error(Str() << "line " << cursor.peek().line << ": unknown directive '" << keyword << "'");
        }
    }
}

// Top level: a sequence of `server { ... }` blocks.
RawConfig parse_config(const std::vector<ConfigToken> &tokens) {
    RawConfig config;
    Cursor cursor(tokens);

    while (!cursor.at_end()) {
        if (!cursor.is_word("server")) {
            throw std::runtime_error(Str() << "line " << cursor.peek().line << ": expected 'server' at top level");
        }
        config.servers.push_back(parse_server(cursor));
    }

    return config;
}
