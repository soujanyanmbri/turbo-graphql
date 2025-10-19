#pragma once
#include <cstdint>
#include <cstring>
#include <immintrin.h>

class CharLookup {
public:
    // Character type bit flags
    static constexpr uint8_t WHITESPACE_FLAG = 1 << 0;
    static constexpr uint8_t DIGIT_FLAG = 1 << 1;
    static constexpr uint8_t IDENTIFIER_FLAG = 1 << 2;
    static constexpr uint8_t SYMBOL_FLAG = 1 << 3;
    static constexpr uint8_t STRING_DELIM_FLAG = 1 << 4;
    static constexpr uint8_t SPECIAL_CHAR_FLAG = 1 << 5;
    static constexpr uint8_t COMMENT_FLAG = 1 << 6;

    // Constructor initializes lookup tables
    CharLookup() {
        initialize();
    }

    // Check if character has a specific flag
    inline bool hasFlag(char c, uint8_t flag) const {
        return (char_type_lut[static_cast<uint8_t>(c)] & flag) != 0;
    }

    // Get token type for a special character
    inline TokenType getSpecialCharType(char c) const {
        return special_char_lut[static_cast<uint8_t>(c)];
    }


private:
    alignas(64) uint8_t char_type_lut[256] = {0};
    alignas(64) TokenType special_char_lut[256] = {(TokenType)0};

    void initialize() {
        std::memset(char_type_lut, 0, sizeof(char_type_lut));
        std::memset(special_char_lut, 0, sizeof(special_char_lut));
        
        // Whitespace - optimize placement in memory for better access patterns
        char_type_lut[' ']  |= WHITESPACE_FLAG;
        char_type_lut['\t'] |= WHITESPACE_FLAG;
        char_type_lut['\n'] |= WHITESPACE_FLAG;
        char_type_lut['\r'] |= WHITESPACE_FLAG;
        
        // Digits
        for (int c = '0'; c <= '9'; c++) {
            char_type_lut[c] |= DIGIT_FLAG | IDENTIFIER_FLAG;
        }
        
        // Identifiers (a-z, A-Z, _)
        for (int c = 'a'; c <= 'z'; c++) {
            char_type_lut[c] |= IDENTIFIER_FLAG;
        }
        for (int c = 'A'; c <= 'Z'; c++) {
            char_type_lut[c] |= IDENTIFIER_FLAG;
        }
        char_type_lut['_'] |= IDENTIFIER_FLAG;
        
        // Comment character
        char_type_lut['/'] |= COMMENT_FLAG;
        char_type_lut['#'] |= COMMENT_FLAG | SYMBOL_FLAG;
        
        // Special characters
        char_type_lut['{'] |= SPECIAL_CHAR_FLAG;
        char_type_lut['}'] |= SPECIAL_CHAR_FLAG;
        char_type_lut['('] |= SPECIAL_CHAR_FLAG;
        char_type_lut[')'] |= SPECIAL_CHAR_FLAG;
        char_type_lut['['] |= SPECIAL_CHAR_FLAG;
        char_type_lut[']'] |= SPECIAL_CHAR_FLAG;
        char_type_lut[':'] |= SPECIAL_CHAR_FLAG;
        char_type_lut[','] |= SPECIAL_CHAR_FLAG;
        char_type_lut['!'] |= SPECIAL_CHAR_FLAG;

        // Map special chars to token types
        special_char_lut['{'] = TokenType::LEFT_BRACE;
        special_char_lut['}'] = TokenType::RIGHT_BRACE;
        special_char_lut['('] = TokenType::LEFT_PAREN;
        special_char_lut[')'] = TokenType::RIGHT_PAREN;
        special_char_lut['['] = TokenType::LEFT_BRACKET;
        special_char_lut[']'] = TokenType::RIGHT_BRACKET;
        special_char_lut[':'] = TokenType::COLON;
        special_char_lut[','] = TokenType::COMMA;
        special_char_lut['!'] = TokenType::EXCLAMATION;
        
        // Other symbols - gather remaining GraphQL symbols
        const char symbols[] = "@!$<>#=+-*/&|^~%?";
        for (char c : symbols) {
            char_type_lut[(uint8_t)c] |= SYMBOL_FLAG;
        }
        
        // String delimiter
        char_type_lut['"'] |= STRING_DELIM_FLAG;
        char_type_lut['\''] |= STRING_DELIM_FLAG;  // Also allow single quotes for some GraphQL implementations
    }
};

inline CharLookup& getCharLookup() {
    static CharLookup instance;
    return instance;
}