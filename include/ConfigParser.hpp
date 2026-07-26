#ifndef CONFIG_PARSER_HPP
# define CONFIG_PARSER_HPP

# include "Config.hpp"
# include <string>
# include <vector>
# include <cstddef>

/*
 * Config lexing and parsing: the token stream, the read cursor, and the
 * tokens -> raw-model build. The raw -> typed step lives in ConfigResolver.hpp.
 */

enum ConfigTokenType { WORD, BLOCK_START, BLOCK_END, TERMINATOR };

typedef struct {
    ConfigTokenType type;
    std::string value;
    size_t line;
} ConfigToken;

// Lex config bytes into a flat token stream.
std::vector<ConfigToken> tokenize(const std::string &config_str);

// Dump the token stream, for debugging.
void                     inspect_tokens(const std::vector<ConfigToken> &tokens);

// Bounds-checked read cursor over a token stream.
class Cursor {
    const std::vector<ConfigToken> &tokens;
    size_t                          pos;

public:
    explicit Cursor(const std::vector<ConfigToken> &tokens);

    // Whether the cursor is past the last token.
    bool                at_end() const;
    // The current token.
    const ConfigToken  &peek() const;
    // Whether the current token is of the given type.
    bool                is(ConfigTokenType type) const;
    // Whether the current token is the given keyword.
    bool                is_word(const char *keyword) const;

    // Consume and return the current token.
    const ConfigToken  &advance();
    // Consume a token of the given type, or throw.
    size_t              expect(ConfigTokenType type);
    // Consume a word token, or throw.
    std::string         expect_word();
    // Consume the given keyword, or throw.
    void                expect_keyword(const char *keyword);
};

// Parse a token stream into the raw config model.
RawConfig parse_config(const std::vector<ConfigToken> &tokens);

#endif
