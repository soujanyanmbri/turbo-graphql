#include "simd/impl/sse_impl.h"
#include "simd/impl/simd_constants.h"
#include <emmintrin.h>
#include <smmintrin.h> // For _mm_popcnt_u32 if needed

size_t SSETextProcessor::skipWhitespace(const char* text, size_t i, size_t textLen) const {
    // Fast path for when we're already at the end
    if (i >= textLen) return i;
    
    // Process 16 bytes at a time
    while (i + 16 <= textLen) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(text + i));
        
        // Generate masks for whitespace characters
        __m128i space_mask = _mm_cmpeq_epi8(chunk, simd_constants::sse_space);
        __m128i tab_mask = _mm_cmpeq_epi8(chunk, simd_constants::sse_tab);
        __m128i nl_mask = _mm_cmpeq_epi8(chunk, simd_constants::sse_nl);
        __m128i cr_mask = _mm_cmpeq_epi8(chunk, simd_constants::sse_cr);
        
        // Combine all masks
        __m128i whitespace_mask = _mm_or_si128(
            _mm_or_si128(space_mask, tab_mask),
            _mm_or_si128(nl_mask, cr_mask)
        );
        
        // Convert mask to bits
        uint16_t ws_bits = _mm_movemask_epi8(whitespace_mask);
        
        // If all bits are set, all characters are whitespace
        if (ws_bits == 0xFFFF) {
            i += 16;
            continue;
        }
        
        // Find the first non-whitespace character
        unsigned int pos = __builtin_ctz(~ws_bits);
        return i + pos;
    }
    
    // Process remaining characters one by one
    while (i < textLen) {
        char c = text[i];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        i++;
    }
    
    return i;
}

size_t SSETextProcessor::skipSingleLineComment(const char* text, size_t i, size_t textLen) const {
    // Skip '//' or '#'
    i += 2; 
    
    // Process 16 bytes at a time
    while (i + 16 <= textLen) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(text + i));
        __m128i nl_mask = _mm_cmpeq_epi8(chunk, simd_constants::sse_nl);
        uint16_t mask = _mm_movemask_epi8(nl_mask);
        
        if (mask) {
            i += __builtin_ctz(mask) + 1; // +1 to move past the newline
            return i;
        }
        
        i += 16;
    }
    
    while (i < textLen && text[i] != '\n') {
        i++;
    }
    
    // Skip the newline if not at end of text
    if (i < textLen) {
        i++;
    }
    
    return i;
}

size_t SSETextProcessor::skipMultiLineComment(const char* text, size_t i, size_t textLen) const {
    // Skip '/*'
    i += 2;
    
    while (i + 16 <= textLen) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(text + i));
        uint16_t mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, simd_constants::sse_star_v));
        
        if (mask) {
            // Try to match '*' followed by '/' in the chunk
            while (mask) {
                uint16_t pos = __builtin_ctz(mask);
                mask &= mask - 1; // Clear the lowest set bit
                
                if (i + pos + 1 < textLen && text[i + pos + 1] == '/') {
                    return i + pos + 2; // Skip past "*/"
                }
            }
        }
        
        i += 15; // Overlap by 1 byte to handle stars at chunk boundaries
    }
    
    // Process remaining characters
    while (i + 1 < textLen) {
        if (text[i] == '*' && text[i + 1] == '/') {
            return i + 2;
        }
        i++;
    }
    // If no closing found, consume to end of text
    return textLen;
}

size_t SSETextProcessor::skipComments(const char* text, size_t i, size_t textLen) const {
    if (i + 1 >= textLen) return i;
    
    // Handle single-line comments: '//' or '#'
    if ((text[i] == '/' && text[i + 1] == '/') || text[i] == '#') {
        return skipSingleLineComment(text, i, textLen);
    }
    
    // Handle multi-line comments: '/*' ... '*/'
    if (text[i] == '/' && text[i + 1] == '*') {
        return skipMultiLineComment(text, i, textLen);
    }
    
    return i; // No comment to skip
}

size_t SSETextProcessor::findIdentifierEnd(const char* text, size_t start, size_t textLen) const {
    if (start >= textLen) return 0;
    
    size_t i = start;
    size_t len = 0;
    
    // Create comparison constants
    const __m128i underscore = _mm_set1_epi8('_');
    const __m128i zero = _mm_set1_epi8('0');
    const __m128i nine = _mm_set1_epi8('9');
    const __m128i a = _mm_set1_epi8('a');
    const __m128i z = _mm_set1_epi8('z');
    const __m128i A = _mm_set1_epi8('A');
    const __m128i Z = _mm_set1_epi8('Z');
    
    while (i + 16 <= textLen) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(text + i));
        
        // Check for identifier characters: a-zA-Z0-9_
        __m128i is_underscore = _mm_cmpeq_epi8(chunk, simd_constants::sse_underscore);
        
        __m128i is_digit = _mm_and_si128(
            _mm_cmpgt_epi8(chunk, _mm_sub_epi8(zero, _mm_set1_epi8(1))),
            _mm_cmpgt_epi8(_mm_add_epi8(nine, _mm_set1_epi8(1)), chunk)
        );
        
        __m128i is_lower = _mm_and_si128(
            _mm_cmpgt_epi8(chunk, _mm_sub_epi8(a, _mm_set1_epi8(1))),
            _mm_cmpgt_epi8(_mm_add_epi8(z, _mm_set1_epi8(1)), chunk)
        );
        
        __m128i is_upper = _mm_and_si128(
            _mm_cmpgt_epi8(chunk, _mm_sub_epi8(A, _mm_set1_epi8(1))),
            _mm_cmpgt_epi8(_mm_add_epi8(Z, _mm_set1_epi8(1)), chunk)
        );
        
        __m128i is_id_char = _mm_or_si128(
            _mm_or_si128(is_underscore, is_digit),
            _mm_or_si128(is_lower, is_upper)
        );
        
        uint16_t id_bits = _mm_movemask_epi8(is_id_char);
        
        if (id_bits == 0xFFFF) {
            i += 16;
            len += 16;
            continue;
        }
        
        // Find first non-identifier character
        unsigned int pos = __builtin_ctz(~id_bits);
        len += pos;
        return len;
    }
    
    // Process remaining characters
    while (i < textLen) {
        char c = text[i];
        if ((c >= 'a' && c <= 'z') || 
            (c >= 'A' && c <= 'Z') || 
            (c >= '0' && c <= '9') || 
            c == '_') {
            i++;
            len++;
        } else {
            break;
        }
    }
    
    return len;
}

size_t SSETextProcessor::findNumberEnd(const char* text, size_t start, size_t textLen, bool& hasDecimal) const {
    if (start >= textLen) return 0;
    
    size_t i = start;
    size_t len = 0;
    hasDecimal = false;
    
    // Process 16 bytes at a time
    while (i + 16 <= textLen) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(text + i));
        
        // Check for digits
        __m128i is_digit = _mm_and_si128(
            _mm_cmpgt_epi8(chunk, _mm_sub_epi8(simd_constants::sse_gt_zero, _mm_set1_epi8(1))),
            _mm_cmpgt_epi8(_mm_add_epi8(simd_constants::sse_lt_nine, _mm_set1_epi8(1)), chunk)
        );
        
        // Handle decimal point separately to find at most one decimal
        __m128i is_decimal = _mm_cmpeq_epi8(chunk, simd_constants::sse_dot);
        uint16_t decimal_bits = _mm_movemask_epi8(is_decimal);
        
        if (!hasDecimal && decimal_bits != 0) {
            hasDecimal = true;
            // Combine with digits for the valid character check
            is_digit = _mm_or_si128(is_digit, is_decimal);
        } else if (hasDecimal && decimal_bits != 0) {
            // Second decimal point - find its position and stop there
            uint16_t pos = __builtin_ctz(decimal_bits);
            len += pos;
            return len;
        }
        
        uint16_t valid_bits = _mm_movemask_epi8(is_digit);
        
        if (valid_bits == 0xFFFF) {
            // All 16 bytes are valid number characters
            i += 16;
            len += 16;
            continue;
        }
        
        // Find first non-numeric character
        unsigned int pos = __builtin_ctz(~valid_bits);
        len += pos;
        return len;
    }
    
    // Process remaining characters
    while (i < textLen) {
        char c = text[i];
        if (c >= '0' && c <= '9') {
            i++;
            len++;
        } else if (c == '.' && !hasDecimal) {
            hasDecimal = true;
            i++;
            len++;
        } else {
            break;
        }
    }
    
    return len;
}

size_t SSETextProcessor::findStringEnd(const char* text, size_t start, size_t textLen, char quoteChar) const {
    if (start >= textLen) return 0;
    
    size_t i = start + 1;  // Skip opening quote
    size_t len = 1;        // Include opening quote in length
    bool escaped = false;
    
    // Create comparison constant for quote character
    const __m128i quote_v = _mm_set1_epi8(quoteChar);
    
    // Process 16 bytes at a time
    while (i + 16 <= textLen && !escaped) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(text + i));
        
        uint16_t quote_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, quote_v));
        uint16_t escape_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, simd_constants::sse_escape));
        
        if (quote_mask == 0 && escape_mask == 0) {
            // No quotes or escapes in this chunk
            i += 16;
            len += 16;
            continue;
        }
        
        // Process all escape sequences first
        while (escape_mask) {
            uint16_t escape_pos = __builtin_ctz(escape_mask);
            escape_mask &= escape_mask - 1;  // Clear the lowest set bit
            
            if (i + escape_pos + 1 < textLen) {
                // Skip the escape char and the escaped char
                len += escape_pos + 2;
                i += escape_pos + 2;
                
                // If we go beyond the chunk, exit the loop
                if (i >= start + 16) {
                    escaped = true;
                    break;
                }
                
                // Recalculate masks based on the new position
                chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(text + i));
                quote_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, quote_v));
                escape_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, simd_constants::sse_escape));
            } else {
                // Escape at end, just move to end of string
                return textLen - start;
            }
        }
        
        if (escaped) {
            escaped = false;
            continue;
        }
        
        // No escapes (or we handled them), now check for quotes
        if (quote_mask) {
            uint16_t quote_pos = __builtin_ctz(quote_mask);
            len += quote_pos + 1;  // +1 to include the quote
            return len;
        }
        
        // If we get here, we had no quotes, continue processing
        i += 16;
        len += 16;
    }
    
    // Process remaining characters
    while (i < textLen) {
        char c = text[i];
        
        if (c == '\\' && !escaped) {
            escaped = true;
            i++;
            len++;
            continue;
        }
        
        if (c == quoteChar && !escaped) {
            // Found closing quote
            return len + 1;  // +1 to include the quote
        }
        
        escaped = false;
        i++;
        len++;
    }
    
    // Return total length up to end of text if closing quote not found
    return len;
}