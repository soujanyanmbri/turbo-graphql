#include "lexer/Lexer.h"
#include "utils/SimdUtils.h"
#include <iostream>
#include <cctype>
#include <bitset>

Tokenizer::Tokenizer(const std::string& input) : input(input), position(0) {}

bool Tokenizer::UseSIMD = true;

std::vector<Token> Tokenizer::tokenize() {
    tokens.clear();
    
    while (position < input.size()) {
        skipWhitespace();

        size_t remaining = input.size() - position;
        if (UseSIMD && remaining >= 32) {
            tokenizeSIMD();
        } else {
            tokenizeFallback();
        }
    }
    return tokens;
}

bool Tokenizer::shouldUseSIMD(const std::string& input, size_t position) {
    return UseSIMD && (input.size() - position) >= 32;
}

void Tokenizer::tokenizeFallback() {
    while (position < input.size()) {
        char c = input[position];

        if (isspace(c)) { 
            position++; 
            continue; 
        }

        switch (c) {
            case '{': tokens.push_back({TokenType::LEFT_BRACE, "{"}); position++; break;
            case '}': tokens.push_back({TokenType::RIGHT_BRACE, "}"}); position++; break;
            case '(': tokens.push_back({TokenType::LEFT_PAREN, "("}); position++; break;
            case ')': tokens.push_back({TokenType::RIGHT_PAREN, ")"}); position++; break;
            case ':': tokens.push_back({TokenType::COLON, ":"}); position++; break;
            case ',': tokens.push_back({TokenType::COMMA, ","}); position++; break;
            case '"': tokens.push_back(parseString()); break;
            default:
                if (isdigit(c) || c == '-') {
                    tokens.push_back(parseNumber());
                } else if (isalpha(c) || c == '_') {
                    tokens.push_back(parseIdentifier());
                } else {
                    tokens.push_back({TokenType::INVALID, std::string(1, c)});
                    position++;
                }
        }
    }
}

void Tokenizer::tokenizeSIMD() {
    constexpr int CHUNK_SIZE = 32;
    
    while (position + CHUNK_SIZE <= input.size()) {
        int mask = 0;

        mask |= SimdUtils::charMask(&input[position], '{') |
                SimdUtils::charMask(&input[position], '}') |
                SimdUtils::charMask(&input[position], '(') |
                SimdUtils::charMask(&input[position], ')') |
                SimdUtils::charMask(&input[position], ':') |
                SimdUtils::charMask(&input[position], ',') |
                SimdUtils::charMask(&input[position], '"');

        if (mask == 0) {  
            position += CHUNK_SIZE; 
        } else {  
            position += __builtin_ctz(mask);
            char c = input[position];

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
                    } else if (isalpha(c) || c == '_') {
                        tokens.push_back(parseIdentifier());
                    } else {
                        tokens.push_back({TokenType::INVALID, std::string(1, c)});
                    }
            }
            position++;
        }
    }
}

Token Tokenizer::parseString() {
    size_t start = position;

    if (input[position] != '"') {
        return {TokenType::INVALID, ""}; 
    }

    position++;

    if (!shouldUseSIMD(input, position)) {
        while (position < input.size() && input[position] != '"') {
            position++;
        }
    } else {
        constexpr int CHUNK_SIZE = 32;
        while (position + CHUNK_SIZE <= input.size()) {
            int mask = SimdUtils::charMask(&input[position], '"');
            
            if (mask == 0) {  
                position += CHUNK_SIZE; 
            } else {  
                position += __builtin_ctz(mask);
                break;
            }
        }
    }
    if (position < input.size() && input[position] == '"') {
        position++;
    } else {
        return {TokenType::INVALID, ""};
    }
    return {TokenType::STRING, input.substr(start + 1, position - start - 2)};
}

Token Tokenizer::parseNumber() {
    size_t start = position;

    if (!shouldUseSIMD(input, position)) {
        while (position < input.size() && (isdigit(input[position]) || input[position] == '.')) {
            position++;
        }
        return {TokenType::NUMBER, input.substr(start, position - start)};
    }

    constexpr int CHUNK_SIZE = 32;

    while (position + CHUNK_SIZE <= input.size()) {
        int mask = SimdUtils::charMask(&input[position], '0') |  
                   SimdUtils::charMask(&input[position], '.');   

        if (mask == 0xFFFFFFFF) {  
            position += CHUNK_SIZE;
        } else {  
            position += __builtin_ctz(~mask);  
            break;
        }
    }

    return {TokenType::NUMBER, input.substr(start, position - start)};
}

Token Tokenizer::parseIdentifier() {
    size_t start = position;

    if (!shouldUseSIMD(input, position)) {
        while (position < input.size() && (isalnum(input[position]) || input[position] == '_')) {
            position++;
        }
        return {TokenType::IDENTIFIER, input.substr(start, position - start)};
    }

    constexpr int CHUNK_SIZE = 32;

    while (position + CHUNK_SIZE <= input.size()) {
        int mask = SimdUtils::charMask(&input[position], 'A') |  
                   SimdUtils::charMask(&input[position], 'a') |  
                   SimdUtils::charMask(&input[position], '0') |  
                   SimdUtils::charMask(&input[position], '_');   

        if (mask == 0xFFFFFFFF) {  
            position += CHUNK_SIZE;
        } else {  
            position += __builtin_ctz(~mask);  
            break;
        }
    }

    return {TokenType::IDENTIFIER, input.substr(start, position - start)};
}

void Tokenizer::skipWhitespace() {
    constexpr int CHUNK_SIZE = 32;

    while (position < input.size()) {
        size_t remaining = input.size() - position;
        
        if (remaining < CHUNK_SIZE) { 
            while (position < input.size() && isspace(input[position])) {
                position++;
            }
            return;
        }

        int mask = SimdUtils::charMask(&input[position], ' ')  |  
                   SimdUtils::charMask(&input[position], '\t') |    
                   SimdUtils::charMask(&input[position], '\n') |    
                   SimdUtils::charMask(&input[position], '\r');    

        if (mask == 0xFFFFFFFF) {  
            position += CHUNK_SIZE;
        } else {  
            position += __builtin_ctz(~mask);
            return;
        }
    }
}
