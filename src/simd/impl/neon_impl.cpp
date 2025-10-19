// #include "simd/impl/neon_impl.h"
// #include "simd/impl/simd_constants.h"
// #include <arm_neon.h>

// size_t NEONTextProcessor::skipWhitespace(const char* text, size_t i, size_t textLen) const {
//     // Fast path for when we're already at the end
//     if (i >= textLen) return i;
    
//     // Process 16 bytes at a time (NEON processes 128-bit vectors instead of 256-bit in AVX2)
//     while (i + 16 <= textLen) {
//         uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(text + i));
        
//         // Generate masks for whitespace characters
//         uint8x16_t space_mask = vceqq_u8(chunk, simd_constants::neon_space);
//         uint8x16_t tab_mask = vceqq_u8(chunk, simd_constants::neon_tab);
//         uint8x16_t nl_mask = vceqq_u8(chunk, simd_constants::neon_nl);
//         uint8x16_t cr_mask = vceqq_u8(chunk, simd_constants::neon_cr);
        
//         // Combine all masks (NEON uses OR instead of _mm256_or_si256)
//         uint8x16_t whitespace_mask = vorrq_u8(
//             vorrq_u8(space_mask, tab_mask),
//             vorrq_u8(nl_mask, cr_mask)
//         );
        
//         // Convert mask to bits (NEON equivalent of _mm256_movemask_epi8)
//         uint16_t ws_bits = 0;
//         for (int j = 0; j < 16; j++) {
//             ws_bits |= (vgetq_lane_u8(whitespace_mask, j) & 0x80) ? (1 << j) : 0;
//         }
        
//         // If all bits are set, all characters are whitespace
//         if (ws_bits == 0xFFFF) {
//             i += 16;
//             continue;
//         }
        
//         // Find the first non-whitespace character
//         unsigned int pos = __builtin_ctz(~ws_bits);
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

// size_t NEONTextProcessor::skipSingleLineComment(const char* text, size_t i, size_t textLen) const {
//     // Skip to end of line
//     i += 2; // Skip '//' or '#'
    
//     // Process 16 bytes at a time
//     while (i + 16 <= textLen) {
//         uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(text + i));
//         uint8x16_t nl_mask = vceqq_u8(chunk, simd_constants::neon_nl);
        
//         // Convert mask to bits
//         uint16_t mask = 0;
//         for (int j = 0; j < 16; j++) {
//             mask |= (vgetq_lane_u8(nl_mask, j) & 0x80) ? (1 << j) : 0;
//         }
        
//         if (mask) {
//             i += __builtin_ctz(mask) + 1; // +1 to move past the newline
//             return i;
//         }
        
//         i += 16;
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

// size_t NEONTextProcessor::skipMultiLineComment(const char* text, size_t i, size_t textLen) const {
//     // Skip '/*'
//     i += 2;
    
//     while (i + 16 <= textLen) {
//         uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(text + i));
//         uint8x16_t star_mask = vceqq_u8(chunk, simd_constants::neon_star);
        
//         // Convert mask to bits
//         uint16_t mask = 0;
//         for (int j = 0; j < 16; j++) {
//             mask |= (vgetq_lane_u8(star_mask, j) & 0x80) ? (1 << j) : 0;
//         }
        
//         if (mask) {
//             // Try to match '*' followed by '/' in the chunk
//             while (mask) {
//                 uint32_t pos = __builtin_ctz(mask);
//                 mask &= mask - 1; // Clear the lowest set bit
                
//                 if (i + pos + 1 < textLen && text[i + pos + 1] == '/') {
//                     return i + pos + 2; // Skip past "*/"
//                 }
//             }
//         }
        
//         i += 15; // Overlap by 1 byte to handle stars at chunk boundaries
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

// size_t NEONTextProcessor::skipComments(const char* text, size_t i, size_t textLen) const {
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

// size_t NEONTextProcessor::findIdentifierEnd(const char* text, size_t start, size_t textLen) const {
//     if (start >= textLen) return 0;
    
//     size_t i = start;
//     size_t len = 0;
    
//     while (i + 16 <= textLen) {
//         uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(text + i));
        
//         // Check for identifier characters: a-zA-Z0-9_
//         uint8x16_t is_underscore = vceqq_u8(chunk, simd_constants::neon_underscore);
        
//         uint8x16_t is_digit = vandq_u8(
//             vcgtq_u8(chunk, simd_constants::neon_gt_zero),
//             vcgtq_u8(simd_constants::neon_lt_nine, chunk)
//         );
        
//         uint8x16_t is_lower = vandq_u8(
//             vcgtq_u8(chunk, simd_constants::neon_gt_a),
//             vcgtq_u8(simd_constants::neon_lt_z, chunk)
//         );
        
//         uint8x16_t is_upper = vandq_u8(
//             vcgtq_u8(chunk, simd_constants::neon_gt_A),
//             vcgtq_u8(simd_constants::neon_lt_Z, chunk)
//         );
        
//         uint8x16_t is_id_char = vorrq_u8(
//             vorrq_u8(is_underscore, is_digit),
//             vorrq_u8(is_lower, is_upper)
//         );
        
//         // Convert mask to bits
//         uint16_t id_bits = 0;
//         for (int j = 0; j < 16; j++) {
//             id_bits |= (vgetq_lane_u8(is_id_char, j) & 0x80) ? (1 << j) : 0;
//         }
        
//         if (id_bits == 0xFFFF) {
//             i += 16;
//             len += 16;
//             continue;
//         }
        
//         // Find first non-identifier character
//         unsigned int pos = __builtin_ctz(~id_bits);
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

// size_t NEONTextProcessor::findNumberEnd(const char* text, size_t start, size_t textLen, bool& hasDecimal) const {
//     if (start >= textLen) return 0;
    
//     size_t i = start;
//     size_t len = 0;
//     hasDecimal = false;
    
//     // Process 16 bytes at a time
//     while (i + 16 <= textLen) {
//         uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(text + i));
        
//         // Check for digits
//         uint8x16_t is_digit = vandq_u8(
//             vcgtq_u8(chunk, simd_constants::neon_gt_zero),
//             vcgtq_u8(simd_constants::neon_lt_nine, chunk)
//         );
        
//         // Handle decimal point separately to find at most one decimal
//         uint8x16_t is_decimal = vceqq_u8(chunk, simd_constants::neon_dot);
        
//         // Convert decimal mask to bits
//         uint16_t decimal_bits = 0;
//         for (int j = 0; j < 16; j++) {
//             decimal_bits |= (vgetq_lane_u8(is_decimal, j) & 0x80) ? (1 << j) : 0;
//         }
        
//         if (!hasDecimal && decimal_bits != 0) {
//             hasDecimal = true;
//             // Combine with digits for the valid character check
//             is_digit = vorrq_u8(is_digit, is_decimal);
//         } else if (hasDecimal && decimal_bits != 0) {
//             // Second decimal point - find its position and stop there
//             uint32_t pos = __builtin_ctz(decimal_bits);
//             len += pos;
//             return len;
//         }
        
//         // Convert valid digit mask to bits
//         uint16_t valid_bits = 0;
//         for (int j = 0; j < 16; j++) {
//             valid_bits |= (vgetq_lane_u8(is_digit, j) & 0x80) ? (1 << j) : 0;
//         }
        
//         if (valid_bits == 0xFFFF) {
//             // All 16 bytes are valid number characters
//             i += 16;
//             len += 16;
//             continue;
//         }
        
//         // Find first non-numeric character
//         unsigned int pos = __builtin_ctz(~valid_bits);
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

// size_t NEONTextProcessor::findStringEnd(const char* text, size_t start, size_t textLen, char quoteChar) const {
//     if (start >= textLen) return 0;
    
//     size_t i = start + 1;  // Skip opening quote
//     size_t len = 1;        // Include opening quote in length
//     bool escaped = false;
    
//     // Setup NEON registers for quote and escape characters
//     uint8x16_t quote_v = vdupq_n_u8(quoteChar);
    
//     // Process 16 bytes at a time
//     while (i + 16 <= textLen && !escaped) {
//         uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(text + i));
        
//         uint8x16_t quote_mask = vceqq_u8(chunk, quote_v);
//         uint8x16_t escape_mask = vceqq_u8(chunk, simd_constants::neon_escape);
        
//         // Convert masks to bits
//         uint16_t quote_bits = 0;
//         uint16_t escape_bits = 0;
//         for (int j = 0; j < 16; j++) {
//             quote_bits |= (vgetq_lane_u8(quote_mask, j) & 0x80) ? (1 << j) : 0;
//             escape_bits |= (vgetq_lane_u8(escape_mask, j) & 0x80) ? (1 << j) : 0;
//         }
        
//         if (quote_bits == 0 && escape_bits == 0) {
//             // No quotes or escapes in this chunk
//             i += 16;
//             len += 16;
//             continue;
//         }
        
//         // Process all escape sequences first
//         while (escape_bits) {
//             uint32_t escape_pos = __builtin_ctz(escape_bits);
//             escape_bits &= escape_bits - 1;  // Clear the lowest set bit
            
//             if (i + escape_pos + 1 < textLen) {
//                 // Skip the escape char and the escaped char
//                 len += escape_pos + 2;
//                 i += escape_pos + 2;
                
//                 // If we go beyond the chunk, exit the loop
//                 if (i >= start + 16) {
//                     escaped = true;
//                     break;
//                 }
                
//                 // Recalculate masks based on the new position
//                 chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(text + i));
//                 quote_mask = vceqq_u8(chunk, quote_v);
//                 escape_mask = vceqq_u8(chunk, simd_constants::neon_escape);
                
//                 // Convert masks to bits again
//                 quote_bits = 0;
//                 escape_bits = 0;
//                 for (int j = 0; j < 16; j++) {
//                     quote_bits |= (vgetq_lane_u8(quote_mask, j) & 0x80) ? (1 << j) : 0;
//                     escape_bits |= (vgetq_lane_u8(escape_mask, j) & 0x80) ? (1 << j) : 0;
//                 }
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
//         if (quote_bits) {
//             uint32_t quote_pos = __builtin_ctz(quote_bits);
//             len += quote_pos + 1;  // +1 to include the quote
//             return len;
//         }
        
//         // If we get here, we had no quotes, continue processing
//         i += 16;
//         len += 16;
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