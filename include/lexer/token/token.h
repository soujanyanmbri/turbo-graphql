#pragma once

#include <string_view>
#include "token_type.h"

/**
 * Token structure representing a lexical unit in GraphQL
 */
struct Token {
    std::string_view value; // 16 bytes
    size_t position;        // 8 bytes
    TokenType type;         // 4 bytes

    Token(TokenType t, std::string_view v, size_t p)
        : type(t), value(v), position(p) {}
        
    // Default constructor
    Token() : type(UNKNOWN), value(), position(0) {}
    
    // Copy constructor
    Token(const Token& other) = default;
    
    // Move constructor
    Token(Token&& other) = default;
    
    // Assignment operators
    Token& operator=(const Token& other) = default;
    Token& operator=(Token&& other) = default;
};