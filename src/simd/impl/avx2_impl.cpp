#include "simd/impl/avx2_impl.h"
#include "simd/impl/simd_constants.h"
#include <immintrin.h>

size_t AVX2TextProcessor::skipWhitespace(const char* text, size_t i, size_t textLen) const {
    // Fast path for when we're already at the end
    if (i >= textLen) return i;
    
    // Process 32 bytes at a time
    while (i + 32 <= textLen) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
        
        // Generate masks for whitespace characters
        __m256i space_mask = _mm256_cmpeq_epi8(chunk, simd_constants::avx2_space);
        __m256i tab_mask = _mm256_cmpeq_epi8(chunk, simd_constants::avx2_tab);
        __m256i nl_mask = _mm256_cmpeq_epi8(chunk, simd_constants::avx2_nl);
        __m256i cr_mask = _mm256_cmpeq_epi8(chunk, simd_constants::avx2_cr);
        
        // Combine all masks
        __m256i whitespace_mask = _mm256_or_si256(
            _mm256_or_si256(space_mask, tab_mask),
            _mm256_or_si256(nl_mask, cr_mask)
        );
        
        // Convert mask to bits
        uint32_t ws_bits = _mm256_movemask_epi8(whitespace_mask);
        
        // If all bits are set, all characters are whitespace
        if (ws_bits == 0xFFFFFFFF) {
            i += 32;
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

size_t AVX2TextProcessor::skipSingleLineComment(const char* text, size_t i, size_t textLen) const {
    // Skip to end of line
    i += 2; // Skip '//' or '#'
    
    // Process 32 bytes at a time
    while (i + 32 <= textLen) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
        __m256i nl_mask = _mm256_cmpeq_epi8(chunk, simd_constants::avx2_nl);
        uint32_t mask = _mm256_movemask_epi8(nl_mask);
        
        if (mask) {
            i += __builtin_ctz(mask) + 1; // +1 to move past the newline
            return i;
        }
        
        i += 32;
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

size_t AVX2TextProcessor::skipMultiLineComment(const char* text, size_t i, size_t textLen) const {
    // Skip '/*'
    i += 2;
    
    const __m256i star_v = _mm256_set1_epi8('*');
    
    while (i + 32 <= textLen) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
        uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, simd_constants::avx2_star_v));
        
        if (mask) {
            // Try to match '*' followed by '/' in the chunk
            while (mask) {
                uint32_t pos = __builtin_ctz(mask);
                mask &= mask - 1; // Clear the lowest set bit
                
                if (i + pos + 1 < textLen && text[i + pos + 1] == '/') {
                    return i + pos + 2; // Skip past "*/"
                }
            }
        }
        
        i += 31; // Overlap by 1 byte to handle stars at chunk boundaries
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

size_t AVX2TextProcessor::skipComments(const char* text, size_t i, size_t textLen) const {
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

size_t AVX2TextProcessor::findIdentifierEnd(const char* text, size_t start, size_t textLen) const {
    if (start >= textLen) return 0;
    
    size_t i = start;
    size_t len = 0;
    
    while (i + 32 <= textLen) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
        
        // Check for identifier characters: a-zA-Z0-9_
        __m256i is_underscore = _mm256_cmpeq_epi8(chunk, simd_constants::avx2_underscore);
        
        __m256i is_digit = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, simd_constants::avx2_gt_zero),
            _mm256_cmpgt_epi8(simd_constants::avx2_lt_nine, chunk)
        );
        
        __m256i is_lower = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, simd_constants::avx2_gt_a),
            _mm256_cmpgt_epi8(simd_constants::avx2_lt_z, chunk)
        );
        
        __m256i is_upper = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, simd_constants::avx2_gt_A),
            _mm256_cmpgt_epi8(simd_constants::avx2_lt_Z, chunk)
        );
        
        __m256i is_id_char = _mm256_or_si256(
            _mm256_or_si256(is_underscore, is_digit),
            _mm256_or_si256(is_lower, is_upper)
        );
        
        uint32_t id_bits = _mm256_movemask_epi8(is_id_char);
        
        if (id_bits == 0xFFFFFFFF) {
            i += 32;
            len += 32;
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

size_t AVX2TextProcessor::findNumberEnd(const char* text, size_t start, size_t textLen, bool& hasDecimal) const {
    if (start >= textLen) return 0;
    
    size_t i = start;
    size_t len = 0;
    hasDecimal = false;
    
    // Process 32 bytes at a time
    while (i + 32 <= textLen) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
        
        // Check for digits
        __m256i is_digit = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, simd_constants::avx2_gt_zero),
            _mm256_cmpgt_epi8(simd_constants::avx2_lt_nine, chunk)
        );
        
        // Handle decimal point separately to find at most one decimal
        __m256i is_decimal = _mm256_cmpeq_epi8(chunk, simd_constants::avx2_dot);
        uint32_t decimal_bits = _mm256_movemask_epi8(is_decimal);
        
        if (!hasDecimal && decimal_bits != 0) {
            hasDecimal = true;
            // Combine with digits for the valid character check
            is_digit = _mm256_or_si256(is_digit, is_decimal);
        } else if (hasDecimal && decimal_bits != 0) {
            // Second decimal point - find its position and stop there
            uint32_t pos = __builtin_ctz(decimal_bits);
            len += pos;
            return len;
        }
        
        uint32_t valid_bits = _mm256_movemask_epi8(is_digit);
        
        if (valid_bits == 0xFFFFFFFF) {
            // All 32 bytes are valid number characters
            i += 32;
            len += 32;
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

size_t AVX2TextProcessor::findStringEnd(const char* text, size_t start, size_t textLen, char quoteChar) const {
    if (start >= textLen) return 0;
    
    size_t i = start + 1;  // Skip opening quote
    size_t len = 1;        // Include opening quote in length
    bool escaped = false;
    
    // Setup SIMD comparisons for quote and escape characters
    const __m256i quote_v = _mm256_set1_epi8(quoteChar);
    
    // Process 32 bytes at a time
    while (i + 32 <= textLen && !escaped) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
        
        uint32_t quote_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, quote_v));
        uint32_t escape_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, simd_constants::avx2_escape));
        
        if (quote_mask == 0 && escape_mask == 0) {
            // No quotes or escapes in this chunk
            i += 32;
            len += 32;
            continue;
        }
        
        // Process all escape sequences first
        while (escape_mask) {
            uint32_t escape_pos = __builtin_ctz(escape_mask);
            escape_mask &= escape_mask - 1;  // Clear the lowest set bit
            
            if (i + escape_pos + 1 < textLen) {
                // Skip the escape char and the escaped char
                len += escape_pos + 2;
                i += escape_pos + 2;
                
                // If we go beyond the chunk, exit the loop
                if (i >= start + 32) {
                    escaped = true;
                    break;
                }
                
                // Recalculate masks based on the new position
                chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
                quote_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, quote_v));
                escape_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, simd_constants::avx2_escape));
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
            uint32_t quote_pos = __builtin_ctz(quote_mask);
            len += quote_pos + 1;  // +1 to include the quote
            return len;
        }
        
        // If we get here, we had no quotes, continue processing
        i += 32;
        len += 32;
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