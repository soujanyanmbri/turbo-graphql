#include "simd/impl/scalar_impl.h"
#include <cstring>
#include <cstdint>

size_t ScalarTextProcessor::skipWhitespace(const char* text, size_t i, size_t textLen) const {
    // Fast path for when we're already at the end
    if (i >= textLen) return i;
    
    const char* ptr = text + i;
    const char* end = text + textLen;
    
    // Process 8 bytes at a time for cache efficiency
    while (ptr + 8 <= end) {
        uint64_t chunk;
        memcpy(&chunk, ptr, sizeof(chunk)); // Avoid unaligned access issues
        
        // Create masks for each whitespace character
        uint64_t space_mask = chunk ^ ~(~0ULL / 255 * ' ');
        uint64_t tab_mask = chunk ^ ~(~0ULL / 255 * '\t');
        uint64_t nl_mask = chunk ^ ~(~0ULL / 255 * '\n');
        uint64_t cr_mask = chunk ^ ~(~0ULL / 255 * '\r');
        
        // Check if each byte is equal to one of the whitespace characters
        uint64_t ws_mask = space_mask & tab_mask & nl_mask & cr_mask;
        
        // If any bit in a byte is set, it's not all zeros (not whitespace)
        if (ws_mask != 0) {
            // Find the first non-whitespace character
            unsigned int offset = 0;
            for (; offset < 8; offset++) {
                char c = ptr[offset];
                if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                    break;
                }
            }
            return (ptr - text) + offset;
        }
        
        ptr += 8;
    }
    
    // Process remaining characters using lookup table
    static const bool isWhitespace[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, // 0-15
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 16-31
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 32-47
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 48-63
        // Rest of the table is all zeros
    };
    
    while (ptr < end) {
        if (!isWhitespace[(unsigned char)*ptr]) {
            break;
        }
        ptr++;
    }
    
    return ptr - text;
}

size_t ScalarTextProcessor::skipSingleLineComment(const char* text, size_t i, size_t textLen) const {
    // Skip '//' or '#'
    i += (text[i] == '#') ? 1 : 2;
    
    const char* ptr = text + i;
    const char* end = text + textLen;
    
    // Use memchr for efficient newline search
    const char* nl = static_cast<const char*>(memchr(ptr, '\n', end - ptr));
    
    if (nl) {
        return (nl - text) + 1; // +1 to move past the newline
    }
    
    return textLen; // No newline found, consume to end
}

size_t ScalarTextProcessor::skipMultiLineComment(const char* text, size_t i, size_t textLen) const {
    // Skip '/*'
    i += 2;
    
    const char* ptr = text + i;
    const char* end = text + textLen - 1; // Need at least 2 chars for "*/"
    
    while (ptr < end) {
        // Search for '*' first
        ptr = static_cast<const char*>(memchr(ptr, '*', end - ptr));
        if (!ptr) {
            return textLen; // No '*' found, consume to end
        }
        
        // Check if '*' is followed by '/'
        if (*(ptr + 1) == '/') {
            return (ptr - text) + 2; // +2 to move past "*/"
        }
        
        ptr++; // Move past this '*'
    }
    
    return textLen; // No closing "*/" found, consume to end
}

size_t ScalarTextProcessor::skipComments(const char* text, size_t i, size_t textLen) const {
    if (i + 1 >= textLen) return i;
    
    char c1 = text[i];
    char c2 = text[i + 1];
    
    // Handle single-line comments: '//' or '#'
    if ((c1 == '/' && c2 == '/') || c1 == '#') {
        return skipSingleLineComment(text, i, textLen);
    }
    
    // Handle multi-line comments: '/*' ... '*/'
    if (c1 == '/' && c2 == '*') {
        return skipMultiLineComment(text, i, textLen);
    }
    
    return i; // No comment to skip
}

size_t ScalarTextProcessor::findIdentifierEnd(const char* text, size_t start, size_t textLen) const {
    if (start >= textLen) return 0;
    
    const char* ptr = text + start;
    const char* end = text + textLen;
    size_t len = 0;
    
    // Precompute a lookup table for valid identifier characters
    static const bool isIdentifierChar[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0-15
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 16-31
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 32-47
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, // 48-57 (digits)
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 64-79 (uppercase)
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, // 80-95 (uppercase + underscore)
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 96-111 (lowercase)
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0  // 112-127 (lowercase)
        // Rest of the table is all zeros
    };
    
    while (ptr < end && isIdentifierChar[(unsigned char)*ptr]) {
        ptr++;
        len++;
    }
    
    return len;
}

size_t ScalarTextProcessor::findNumberEnd(const char* text, size_t start, size_t textLen, bool& hasDecimal) const {
    if (start >= textLen) return 0;
    
    const char* ptr = text + start;
    const char* end = text + textLen;
    size_t len = 0;
    hasDecimal = false;
    
    // Check for digits and decimal point
    while (ptr < end) {
        char c = *ptr;
        
        if (c >= '0' && c <= '9') {
            ptr++;
            len++;
        } else if (c == '.' && !hasDecimal) {
            hasDecimal = true;
            ptr++;
            len++;
        } else {
            break;
        }
    }
    
    return len;
}

size_t ScalarTextProcessor::findStringEnd(const char* text, size_t start, size_t textLen, char quoteChar) const {
    if (start >= textLen) return 0;
    
    const char* ptr = text + start + 1;  // Skip opening quote
    const char* end = text + textLen;
    size_t len = 1;  // Include opening quote in length
    bool escaped = false;
    
    while (ptr < end) {
        char c = *ptr;
        
        if (escaped) {
            // Any escaped character is part of the string
            escaped = false;
            ptr++;
            len++;
            continue;
        }
        
        if (c == '\\') {
            escaped = true;
            ptr++;
            len++;
            continue;
        }
        
        if (c == quoteChar) {
            // Found closing quote
            return len + 1;  // +1 to include the quote
        }
        
        ptr++;
        len++;
    }
    
    // Return total length up to end of text if closing quote not found
    return len;
}

// // Additional optimization: Batched token processing
// void ScalarTextProcessor::processTokens(const char* text, size_t textLen, TokenCallback callback) const {
//     size_t i = 0;
    
//     // Use a jump table for fast dispatch based on character
//     static const unsigned char TYPE_WHITESPACE = 0;
//     static const unsigned char TYPE_ALPHA = 1;
//     static const unsigned char TYPE_DIGIT = 2;
//     static const unsigned char TYPE_QUOTE = 3;
//     static const unsigned char TYPE_COMMENT = 4;
//     static const unsigned char TYPE_OTHER = 5;
    
//     static const unsigned char charTypes[256] = {
//         5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 5, 5, 0, 5, 5, // 0-15
//         5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 16-31
//         0, 5, 3, 4, 5, 5, 5, 3, 5, 5, 5, 5, 5, 5, 5, 4, // 32-47 (space, ", #, /)
//         2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 5, 5, 5, 5, 5, 5, // 48-63 (digits)
//         5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 64-79 (uppercase)
//         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 5, 5, 5, 1, // 80-95 (uppercase + underscore)
//         5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 96-111 (lowercase)
//         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 5, 5, 5, 5  // 112-127 (lowercase)
//         // Rest default to TYPE_OTHER (5)
//     };
    
//     while (i < textLen) {
//         unsigned char type = charTypes[(unsigned char)text[i]];
//         size_t tokenStart = i;
//         size_t tokenLen = 0;
//         TokenType tokenType;
        
//         switch (type) {
//             case TYPE_WHITESPACE:
//                 i = skipWhitespace(text, i, textLen);
//                 continue;  // Skip whitespace
                
//             case TYPE_ALPHA:
//                 tokenLen = findIdentifierEnd(text, i, textLen);
//                 tokenType = TOKEN_IDENTIFIER;
//                 break;
                
//             case TYPE_DIGIT: {
//                 bool hasDecimal;
//                 tokenLen = findNumberEnd(text, i, textLen, hasDecimal);
//                 tokenType = TOKEN_NUMBER;
//                 break;
//             }
                
//             case TYPE_QUOTE:
//                 tokenLen = findStringEnd(text, i, textLen, text[i]);
//                 tokenType = TOKEN_STRING;
//                 break;
                
//             case TYPE_COMMENT:
//                 // Check if it's actually a comment
//                 if (i + 1 < textLen && 
//                     ((text[i] == '/' && (text[i+1] == '/' || text[i+1] == '*')) || 
//                      text[i] == '#')) {
//                     i = skipComments(text, i, textLen);
//                     continue;  // Skip comments
//                 }
//                 // Fall through if not a comment
                
//             case TYPE_OTHER:
//             default:
//                 tokenLen = 1;  // Single character token
//                 tokenType = TOKEN_SYMBOL;
//                 break;
//         }
        
//         // Call the callback with the token
//         if (tokenLen > 0) {
//             callback(text + tokenStart, tokenLen, tokenType);
//             i += tokenLen;
//         } else {
//             i++;  // Safeguard against infinite loops
//         }
//     }
// }