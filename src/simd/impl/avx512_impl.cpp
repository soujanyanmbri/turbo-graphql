// #include "simd/impl/avx512_impl.h"
// #include "simd/impl/simd_constants.h"
// #include <immintrin.h>

// size_t AVX512TextProcessor::skipWhitespace(const char* text, size_t i, size_t textLen) const {
//     // Fast path for when we're already at the end
//     if (i >= textLen) return i;
    
//     // Process 64 bytes at a time
//     while (i + 64 <= textLen) {
//         __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(text + i));
        
//         // Generate masks for whitespace characters
//         __mmask64 space_mask = _mm512_cmpeq_epi8_mask(chunk, simd_constants::avx512_space);
//         __mmask64 tab_mask = _mm512_cmpeq_epi8_mask(chunk, simd_constants::avx512_tab);
//         __mmask64 nl_mask = _mm512_cmpeq_epi8_mask(chunk, simd_constants::avx512_nl);
//         __mmask64 cr_mask = _mm512_cmpeq_epi8_mask(chunk, simd_constants::avx512_cr);
        
//         // Combine all masks
//         __mmask64 whitespace_mask = space_mask | tab_mask | nl_mask | cr_mask;
        
//         // If all bits are set, all characters are whitespace
//         if (whitespace_mask == 0xFFFFFFFFFFFFFFFF) {
//             i += 64;
//             continue;
//         }
        
//         // Find the first non-whitespace character
//         unsigned int pos = _tzcnt_u64(~whitespace_mask);
//         return i + pos;
//     }
    
//     // Process remaining characters one by one
//     while (i < textLen) {
//         char c = text[i];
//         if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
//             break;
//         }
//         i++;
//     }
    
//     return i;
// }

// size_t AVX512TextProcessor::skipSingleLineComment(const char* text, size_t i, size_t textLen) const {
//     // Skip '//' or '#'
//     i += 2; 
    
//     // Process 64 bytes at a time
//     while (i + 64 <= textLen) {
//         __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(text + i));
//         __mmask64 nl_mask = _mm512_cmpeq_epi8_mask(chunk, simd_constants::avx512_nl);
        
//         if (nl_mask) {
//             i += _tzcnt_u64(nl_mask) + 1; // +1 to move past the newline
//             return i;
//         }
        
//         i += 64;
//     }
    
//     while (i < textLen && text[i] != '\n') {
//         i++;
//     }
    
//     // Skip the newline if not at end of text
//     if (i < textLen) {
//         i++;
//     }
    
//     return i;
// }

// size_t AVX512TextProcessor::skipMultiLineComment(const char* text, size_t i, size_t textLen) const {
//     // Skip '/*'
//     i += 2;
    
//     while (i + 64 <= textLen) {
//         __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(text + i));
//         __mmask64 star_mask = _mm512_cmpeq_epi8_mask(chunk, simd_constants::avx512_star_v);
        
//         if (star_mask) {
//             // Try to match '*' followed by '/' in the chunk
//             uint64_t mask = star_mask;
//             while (mask) {
//                 uint32_t pos = _tzcnt_u64(mask);
//                 mask &= mask - 1; // Clear the lowest set bit
                
//                 if (i + pos + 1 < textLen && text[i + pos + 1] == '/') {
//                     return i + pos + 2; // Skip past "*/"
//                 }
//             }
//         }
        
//         i += 63; // Overlap by 1 byte to handle stars at chunk boundaries
//     }
    
//     // Process remaining characters
//     while (i + 1 < textLen) {
//         if (text[i] == '*' && text[i + 1] == '/') {
//             return i + 2;
//         }
//         i++;
//     }
//     // If no closing found, consume to end of text
//     return textLen;
// }

// size_t AVX512TextProcessor::skipComments(const char* text, size_t i, size_t textLen) const {
//     if (i + 1 >= textLen) return i;
    
//     // Handle single-line comments: '//' or '#'
//     if ((text[i] == '/' && text[i + 1] == '/') || text[i] == '#') {
//         return skipSingleLineComment(text, i, textLen);
//     }
    
//     // Handle multi-line comments: '/*' ... '*/'
//     if (text[i] == '/' && text[i + 1] == '*') {
//         return skipMultiLineComment(text, i, textLen);
//     }
    
//     return i; // No comment to skip
// }

// size_t AVX512TextProcessor::findIdentifierEnd(const char* text, size_t start, size_t textLen) const {
//     if (start >= textLen) return 0;
    
//     size_t i = start;
//     size_t len = 0;
    
//     while (i + 64 <= textLen) {
//         __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(text + i));
        
//         // Check for identifier characters: a-zA-Z0-9_
//         __mmask64 is_underscore = _mm512_cmpeq_epi8_mask(chunk, simd_constants::avx512_underscore);
        
//         __mmask64 is_digit = _mm512_mask_cmpge_epi8_mask(0xFFFFFFFFFFFFFFFF, chunk, simd_constants::avx512_gt_zero) &
//                              _mm512_mask_cmple_epi8_mask(0xFFFFFFFFFFFFFFFF, chunk, simd_constants::avx512_lt_nine);
        
//         __mmask64 is_lower = _mm512_mask_cmpge_epi8_mask(0xFFFFFFFFFFFFFFFF, chunk, simd_constants::avx512_gt_a) &
//                              _mm512_mask_cmple_epi8_mask(0xFFFFFFFFFFFFFFFF, chunk, simd_constants::avx512_lt_z);
        
//         __mmask64 is_upper = _mm512_mask_cmpge_epi8_mask(0xFFFFFFFFFFFFFFFF, chunk, simd_constants::avx512_gt_A) &
//                              _mm512_mask_cmple_epi8_mask(0xFFFFFFFFFFFFFFFF, chunk, simd_constants::avx512_lt_Z);
        
//         __mmask64 is_id_char = is_underscore | is_digit | is_lower | is_upper;
        
//         if (is_id_char == 0xFFFFFFFFFFFFFFFF) {
//             i += 64;
//             len += 64;
//             continue;
//         }
        
//         // Find first non-identifier character
//         unsigned int pos = _tzcnt_u64(~is_id_char);
//         len += pos;
//         return len;
//     }
    
//     // Process remaining characters
//     while (i < textLen) {
//         char c = text[i];
//         if ((c >= 'a' && c <= 'z') || 
//             (c >= 'A' && c <= 'Z') || 
//             (c >= '0' && c <= '9') || 
//             c == '_') {
//             i++;
//             len++;
//         } else {
//             break;
//         }
//     }
    
//     return len;
// }

// size_t AVX512TextProcessor::findNumberEnd(const char* text, size_t start, size_t textLen, bool& hasDecimal) const {
//     if (start >= textLen) return 0;
    
//     size_t i = start;
//     size_t len = 0;
//     hasDecimal = false;
    
//     // Process 64 bytes at a time
//     while (i + 64 <= textLen) {
//         __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(text + i));
        
//         // Check for digits
//         __mmask64 is_digit = _mm512_mask_cmpge_epi8_mask(0xFFFFFFFFFFFFFFFF, chunk, simd_constants::avx512_gt_zero) &
//                              _mm512_mask_cmple_epi8_mask(0xFFFFFFFFFFFFFFFF, chunk, simd_constants::avx512_lt_nine);
        
//         // Handle decimal point separately to find at most one decimal
//         __mmask64 is_decimal = _mm512_cmpeq_epi8_mask(chunk, simd_constants::avx512_dot);
        
//         if (!hasDecimal && is_decimal) {
//             hasDecimal = true;
//             // Combine with digits for the valid character check
//             is_digit |= is_decimal;
//         } else if (hasDecimal && is_decimal) {
//             // Second decimal point - find its position and stop there
//             unsigned int pos = _tzcnt_u64(is_decimal);
//             len += pos;
//             return len;
//         }
        
//         if (is_digit == 0xFFFFFFFFFFFFFFFF) {
//             // All 64 bytes are valid number characters
//             i += 64;
//             len += 64;
//             continue;
//         }
        
//         // Find first non-numeric character
//         unsigned int pos = _tzcnt_u64(~is_digit);
//         len += pos;
//         return len;
//     }
    
//     // Process remaining characters
//     while (i < textLen) {
//         char c = text[i];
//         if (c >= '0' && c <= '9') {
//             i++;
//             len++;
//         } else if (c == '.' && !hasDecimal) {
//             hasDecimal = true;
//             i++;
//             len++;
//         } else {
//             break;
//         }
//     }
    
//     return len;
// }

// size_t AVX512TextProcessor::findStringEnd(const char* text, size_t start, size_t textLen, char quoteChar) const {
//     if (start >= textLen) return 0;
    
//     size_t i = start + 1;  // Skip opening quote
//     size_t len = 1;        // Include opening quote in length
//     bool escaped = false;
    
//     // Process 64 bytes at a time
//     while (i + 64 <= textLen && !escaped) {
//         __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(text + i));
        
//         __m512i quote_v = _mm512_set1_epi8(quoteChar);
//         __mmask64 quote_mask = _mm512_cmpeq_epi8_mask(chunk, quote_v);
//         __mmask64 escape_mask = _mm512_cmpeq_epi8_mask(chunk, simd_constants::avx512_escape);
        
//         if (quote_mask == 0 && escape_mask == 0) {
//             // No quotes or escapes in this chunk
//             i += 64;
//             len += 64;
//             continue;
//         }
        
//         // Process all escape sequences first
//         uint64_t esc_bits = escape_mask;
//         while (esc_bits) {
//             uint32_t escape_pos = _tzcnt_u64(esc_bits);
//             esc_bits &= esc_bits - 1;  // Clear the lowest set bit
            
//             if (i + escape_pos + 1 < textLen) {
//                 // Skip the escape char and the escaped char
//                 len += escape_pos + 2;
//                 i += escape_pos + 2;
                
//                 // If we go beyond the chunk, exit the loop
//                 if (i >= start + 64) {
//                     escaped = true;
//                     break;
//                 }
                
//                 // Recalculate masks based on the new position
//                 chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(text + i));
//                 quote_mask = _mm512_cmpeq_epi8_mask(chunk, quote_v);
//                 escape_mask = _mm512_cmpeq_epi8_mask(chunk, simd_constants::avx512_escape);
//                 esc_bits = escape_mask;
//             } else {
//                 // Escape at end, just move to end of string
//                 return textLen - start;
//             }
//         }
        
//         if (escaped) {
//             escaped = false;
//             continue;
//         }
        
//         // No escapes (or we handled them), now check for quotes
//         if (quote_mask) {
//             uint32_t quote_pos = _tzcnt_u64(quote_mask);
//             len += quote_pos + 1;  // +1 to include the quote
//             return len;
//         }
        
//         // If we get here, we had no quotes, continue processing
//         i += 64;
//         len += 64;
//     }
    
//     // Process remaining characters
//     while (i < textLen) {
//         char c = text[i];
        
//         if (c == '\\' && !escaped) {
//             escaped = true;
//             i++;
//             len++;
//             continue;
//         }
        
//         if (c == quoteChar && !escaped) {
//             // Found closing quote
//             return len + 1;  // +1 to include the quote
//         }
        
//         escaped = false;
//         i++;
//         len++;
//     }
    
//     // Return total length up to end of text if closing quote not found
//     return len;
// }