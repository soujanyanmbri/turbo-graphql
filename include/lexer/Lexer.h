#ifndef GRAPHQL_LEXER_H
#define GRAPHQL_LEXER_H

#include <string>
#include <vector>

enum class TokenType {
    // Structural tokens
    LEFT_BRACE,      // {
    RIGHT_BRACE,     // }
    LEFT_PAREN,      // (
    RIGHT_PAREN,     // )
    LEFT_BRACKET,    // [
    RIGHT_BRACKET,   // ]
    COLON,           // :
    COMMA,           // ,
    ELLIPSIS,        // ...

    // Keywords & Identifiers
    OPERATION,       // query, mutation, subscription
    IDENTIFIER,      // Field, variable, type name
    TYPE_KEYWORD,    // type, enum, interface, union, input, scalar
    DIRECTIVE,       // @include, @skip

    // Literals
    STRING,          // "hello"
    NUMBER,          // 123, 3.14
    BOOLEAN,         // true, false
    NULL_VALUE,      // null

    // Special Symbols
    DOLLAR,          // $ 
    EQUAL,           // =
    PIPE,            // |

    // End of input
    END
};

// Token structure
struct Token {
    TokenType type;
    std::string value;
};

// Tokenizer class
class Tokenizer {
public:
    explicit Tokenizer(const std::string& input);
    Token getNextToken();
    
    std::vector<Token> tokenize();


private:
    std::string input;
    size_t position;

    /*
    Tokensization methods: One for the usual, and one for the fallback:
        1. tokenizeSIMD
        2. tokenizeFallback
    Where to pick which one:
        1. GLOBAL config UseSIMD - Tells us whether to use SIMD or not
        2. Based on the folowing conditions, pick the appropriate method:
            a. If the size of the string is small (<32 Bytes) switch to scalar impl because SIMD registers are 32-64 bytes based on the CPU you're using. 
            b. If there are too many symbols or short strings (token density) - switch to scalar impl
            c. Escape heavy strings 
    */

    void tokenizeSIMD();
    void tokenizeFallback();

    char peek() const;
    char advance();
    void skipWhitespace();
    void skipComment();
    Token parseNumber();
    Token parseString();
    Token parseIdentifier();
};

#endif // GRAPHQL_LEXER_H
