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
                    Token boolToken = parseBoolean();
                    if (boolToken.type != TokenType::INVALID) {
                        tokens.push_back(boolToken);
                    } else {
                        tokens.push_back(parseIdentifier());
                    }
                } else {
                    tokens.push_back({TokenType::INVALID, std::string(1, c)});
                    position++;
                }
        }
    }
}

void Tokenizer::tokenizeSIMD() {
    constexpr int CHUNK_SIZE = 32;
    
    skipWhitespace();
    
    if (position < input.size()) {
        tokenizeFallback();
    }
    
    while (position + CHUNK_SIZE <= input.size()) {
        skipWhitespace();
        
        int mask = SimdUtils::charMask(&input[position], '{') |
                   SimdUtils::charMask(&input[position], '}') |
                   SimdUtils::charMask(&input[position], '(') |
                   SimdUtils::charMask(&input[position], ')') |
                   SimdUtils::charMask(&input[position], ':') |
                   SimdUtils::charMask(&input[position], ',') |
                   SimdUtils::charMask(&input[position], '"');

        if (mask == 0) {  
            position += CHUNK_SIZE; 
            continue;
        } 
        
        position += __builtin_ctz(mask);
        char c = input[position];

        switch (c) {
            case '{': tokens.push_back({TokenType::LEFT_BRACE, "{"}); position++; break;
            case '}': tokens.push_back({TokenType::RIGHT_BRACE, "}"}); position++; break;
            case '(': tokens.push_back({TokenType::LEFT_PAREN, "("}); position++; break;
            case ')': tokens.push_back({TokenType::RIGHT_PAREN, ")"}); position++; break;
            case ':': tokens.push_back({TokenType::COLON, ":"}); position++; break;
            case ',': tokens.push_back({TokenType::COMMA, ","}); position++; break;
            case '"': 
                tokens.push_back(parseString()); 
                break;
            default:
                if (isdigit(c) || c == '-') {
                    tokens.push_back(parseNumber());
                } else if (isalpha(c) || c == '_') {
                    Token boolToken = parseBoolean();
                    if (boolToken.type != TokenType::INVALID) {
                        tokens.push_back(boolToken);
                    } else {
                        tokens.push_back(parseIdentifier());
                    }
                } else {
                    tokens.push_back({TokenType::INVALID, std::string(1, c)});
                    position++;
                }
        }
    }
    
    // Fall back to regular tokenization for remaining characters
    tokenizeFallback();
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
        
        // Handle remaining characters using non-SIMD approach
        while (position < input.size() && input[position] != '"') {
            position++;
        }
    }
    
    if (position < input.size() && input[position] == '"') {
        position++;
        return {TokenType::STRING, input.substr(start + 1, position - start - 2)};
    } else {
        return {TokenType::INVALID, input.substr(start, position - start)};
    }
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
        // Create a mask where bits are set for positions that are NOT digits or dots
        int digit_mask = 0;
        for (int i = 0; i < 10; i++) {
            digit_mask |= SimdUtils::charMask(&input[position], '0' + i);
        }
        int dot_mask = SimdUtils::charMask(&input[position], '.');
        int valid_mask = digit_mask | dot_mask;
        
        // If all characters in the chunk are valid number characters
        if (valid_mask == 0xFFFFFFFF) {
            position += CHUNK_SIZE;
        } else {
            // Find the first non-digit, non-dot character
            position += __builtin_ctz(~valid_mask);
            break;
        }
    }

    // Process any remaining characters
    while (position < input.size() && (isdigit(input[position]) || input[position] == '.')) {
        position++;
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
        // Create masks for all valid identifier characters
        int alpha_lower_mask = 0;
        int alpha_upper_mask = 0;
        int digit_mask = 0;
        
        for (int i = 0; i < 26; i++) {
            alpha_lower_mask |= SimdUtils::charMask(&input[position], 'a' + i);
            alpha_upper_mask |= SimdUtils::charMask(&input[position], 'A' + i);
        }
        
        for (int i = 0; i < 10; i++) {
            digit_mask |= SimdUtils::charMask(&input[position], '0' + i);
        }
        
        int underscore_mask = SimdUtils::charMask(&input[position], '_');
        int valid_mask = alpha_lower_mask | alpha_upper_mask | digit_mask | underscore_mask;
        
        // If all characters in the chunk are valid identifier characters
        if (valid_mask == 0xFFFFFFFF) {
            position += CHUNK_SIZE;
        } else {
            // Find the first invalid character
            position += __builtin_ctz(~valid_mask);
            break;
        }
    }

    // Process any remaining characters
    while (position < input.size() && (isalnum(input[position]) || input[position] == '_')) {
        position++;
    }

    return {TokenType::IDENTIFIER, input.substr(start, position - start)};
}

Token Tokenizer::parseBoolean() {
    size_t start = position;
    
    // Check if there are enough characters left
    if (position + 4 <= input.size() && input.compare(position, 4, "true") == 0) {
        // Check if the next character after "true" is not alphanumeric (complete word check)
        if (position + 4 == input.size() || !(isalnum(input[position + 4]) || input[position + 4] == '_')) {
            position += 4;
            return {TokenType::BOOLEAN, "true"};
        }
    } else if (position + 5 <= input.size() && input.compare(position, 5, "false") == 0) {
        // Check if the next character after "false" is not alphanumeric (complete word check)
        if (position + 5 == input.size() || !(isalnum(input[position + 5]) || input[position + 5] == '_')) {
            position += 5;
            return {TokenType::BOOLEAN, "false"};
        }
    }
    
    return {TokenType::INVALID, ""};
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