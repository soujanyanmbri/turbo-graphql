#include "lexer/Lexer.h"
#include <iostream>
#include <cctype>
#include <bitset>


// Constructor: Initialize the lexer with the input string, starting at position 0.
Tokenizer::Tokenizer(const std::string& input) : input(input), position(0) {}

// Global config - to use SIMD or not:
bool Tokenizer::UseSIMD = true;

std::vector<Token> Tokenizer::tokenize() {
    tokens.clear();
    
    while (position < input.size()) {
        size_t remaining = input.size() - position;
        if (UseSIMD && remaining >= 32) {
            tokenizeSIMD();
        } else {
            tokenizeFallback();
        }
    }
    return tokens;
}

void Tokenizer::tokenizeFallback() {
    while (position < input.size()) {
        char c = input[position++];
        if (isspace(c)) continue;

        switch (c) {
            case '{': tokens.push_back({TokenType::LEFT_BRACE, "{"}); break;
            case '}': tokens.push_back({TokenType::RIGHT_BRACE, "}"}); break;
            case '(': tokens.push_back({TokenType::LEFT_PAREN, "("}); break;
            case ')': tokens.push_back({TokenType::RIGHT_PAREN, ")"}); break;
            case ':': tokens.push_back({TokenType::COLON, ":"}); break;
            case ',': tokens.push_back({TokenType::COMMA, ","}); break;
            case '"': tokens.push_back(parseString()); break;
            default:
                if (isdigit(c) || c == '-') {
                    tokens.push_back(parseNumber());
                } else if (isalpha(c)) {
                    tokens.push_back(parseIdentifier());
                }
        }
    }
}

void Tokenizer::tokenizeSIMD() {
    
}