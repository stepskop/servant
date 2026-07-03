#ifndef CONFIG_PARSER_HPP
# define CONFIG_PARSER_HPP

# include "Config.hpp"
# include <string>
# include <vector>
# include <cstddef>

// Config lexing + parsing surface: the token stream, the read cursor, and the
// tokens -> raw-model build. The raw -> typed step lives in ConfigResolver.hpp.

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

// Stage 1: bytes -> flat token stream (Tokenizer.cpp).
std::vector<ConfigToken> tokenize(const std::string &config_str);
void                     inspect_tokens(const std::vector<ConfigToken> &tokens);

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

// Stage 2a: tokens -> raw model, verbatim, no defaults (ConfigParser.cpp).
RawConfig parse_config(const std::vector<ConfigToken> &tokens);

#endif
