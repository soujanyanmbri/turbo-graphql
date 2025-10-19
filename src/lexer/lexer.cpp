#include <string_view>
#include <vector>
#include <memory_resource>
#include <memory>
#include <immintrin.h>
#include "lexer/lexer.h"
#include "lexer/token/token_arena.h"
#include "lexer/character_classifier.h"
#include "lexer/keyword_classifier.h"
#include "simd/simd_factory.h"
#include "simd/simd_interface.h"

// Pre-compute SIMD constants for identifier checking
const __m256i underscore = _mm256_set1_epi8('_');
const __m256i gt_zero = _mm256_set1_epi8('0' - 1);
const __m256i lt_nine = _mm256_set1_epi8('9' + 1);
const __m256i gt_a = _mm256_set1_epi8('a' - 1);
const __m256i lt_z = _mm256_set1_epi8('z' + 1);
const __m256i gt_A = _mm256_set1_epi8('A' - 1);
const __m256i lt_Z = _mm256_set1_epi8('Z' + 1);

const __m256i space = _mm256_set1_epi8(' ');
const __m256i tab = _mm256_set1_epi8('\t');
const __m256i nl = _mm256_set1_epi8('\n');
const __m256i cr = _mm256_set1_epi8('\r');
const __m256i star_v = _mm256_set1_epi8('*');

// Fast SIMD-based comment skipping (optimized)
__attribute__((always_inline)) inline size_t skip_comments_simd(const char* __restrict__ text, size_t i, size_t text_len) {
  if (i + 1 >= text_len) return i;
  if ((text[i] == '/' && i + 1 < text_len && text[i + 1] == '/') || text[i] == '#') {
    i += (text[i] == '/' ? 2 : 1);

    const __m256i nl = _mm256_set1_epi8('\n');

    while (i + 32 <= text_len) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
        __m256i nl_mask = _mm256_cmpeq_epi8(chunk, nl);
        uint32_t mask = _mm256_movemask_epi8(nl_mask);

        if (mask) {
            i += __builtin_ctz(mask) + 1;
            return i;
        }

        i += 32;
    }

    while (i < text_len && text[i] != '\n') ++i;
    if (i < text_len) ++i;
    return i;
  }

  if (text[i] == '/' && text[i + 1] == '*') {
      i += 2;
      size_t last_star_check = i;

      while (i + 32 <= text_len) {
          __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
          uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, star_v));

          if (mask) {
              // Try to match '*' followed by '/' in the same chunk
              while (mask) {
                  uint32_t pos = __builtin_ctz(mask);
                  mask &= mask - 1;

                  if (i + pos + 1 < text_len && text[i + pos + 1] == '/') {
                      return i + pos + 2;
                  }
              }
          }

          // FIX: Advance from last checked position to avoid missing boundary case
          last_star_check = i + 32;
          i += 32;
      }
      
      // Set i back to continue from last star check
      i = last_star_check;

      // Final tail scan
      while (i + 1 < text_len) {
          if (text[i] == '*' && text[i + 1] == '/') {
              return i + 2;
          }
          ++i;
      }

      return text_len;
  }

  return i;
}

// Optimized tokenizer with better SIMD usage and branch prediction
std::pmr::vector<Token>& Tokenizer::tokenize(const char* text, 
    size_t text_len, 
    TokenArena& arena) {
    
    // Pre-allocate with exact size for small documents or a reasonable estimate for larger ones
    std::pmr::vector<Token>& tokens = arena.tokens_vector;
    tokens.clear();  // Ensure we have a clean vector
    tokens.reserve(text_len > 1000 ? text_len / 3 : text_len);
    
    size_t i = 0;
    
    // Skip BOM if present (common in some GraphQL files)
    if (text_len >= 3 && 
        static_cast<unsigned char>(text[0]) == 0xEF && 
        static_cast<unsigned char>(text[1]) == 0xBB && 
        static_cast<unsigned char>(text[2]) == 0xBF) {
        i = 3;
    }

    while (i < text_len) {
        // SIMD-accelerated whitespace skipping
        if (__builtin_expect(i + 32 <= text_len, 1)) {
            while (i + 32 <= text_len) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
                
                // Generate masks for whitespace characters
                __m256i space_mask = _mm256_cmpeq_epi8(chunk, space);
                __m256i tab_mask = _mm256_cmpeq_epi8(chunk, tab);
                __m256i nl_mask = _mm256_cmpeq_epi8(chunk, nl);
                __m256i cr_mask = _mm256_cmpeq_epi8(chunk, cr);
                
                // Combine masks
                __m256i whitespace_mask = _mm256_or_si256(
                    _mm256_or_si256(space_mask, tab_mask),
                    _mm256_or_si256(nl_mask, cr_mask)
                );
                
                uint32_t ws_bits = _mm256_movemask_epi8(whitespace_mask);
                
                // FIX: Only break if we found non-whitespace
                if (ws_bits != 0xFFFFFFFF) {
                    unsigned int skip = __builtin_ctz(~ws_bits);
                    i += skip;
                    break;
                }
                // All whitespace, continue to next chunk
                i += 32;
            }
        } 
        
        // Handle remaining whitespace with LUT
        while (i < text_len && getCharLookup().hasFlag(text[i], CharLookup::WHITESPACE_FLAG)) {
            i++;
        }
        
        if (i >= text_len) break;
        
        char c = text[i];
        
        // Fast path for comment detection (//, /*, or #)
        if (__builtin_expect((c == '#') || (c == '/' && i + 1 < text_len && 
        (text[i + 1] == '/' || text[i + 1] == '*')), 0)) {
                i = skip_comments_simd(text, i, text_len);
                continue;
        }
        
        // Check for ellipsis next
        if (i + 2 < text_len && text[i] == '.' && text[i + 1] == '.' && text[i + 2] == '.') {
            tokens.emplace_back(TokenType::ELLIPSIS, std::string_view(&text[i], 3), i);
            i += 3;
            continue;
        }
        
        // Check for variables ($) and directives (@)
        if ((c == '$' || c == '@') && i + 1 < text_len && 
            getCharLookup().hasFlag(text[i + 1], CharLookup::IDENTIFIER_FLAG)) {
            
            TokenType type = (c == '$') ? TokenType::VARIABLE : TokenType::DIRECTIVE;
            size_t start = i++;
            size_t len = 1;
        
            while (i < text_len && getCharLookup().hasFlag(text[i], CharLookup::IDENTIFIER_FLAG)) {
                i++;
                len++;
            }
        
            tokens.emplace_back(type, std::string_view(text + start, len), start);
            continue;
        }
        
        // Fast path for special characters
        if (__builtin_expect(getCharLookup().hasFlag(c, CharLookup::SPECIAL_CHAR_FLAG), 1)) {
            TokenType type = getCharLookup().getSpecialCharType(c);
            tokens.emplace_back(type, std::string_view(text + i, 1), i);
            i++;
            continue;
        }

        // Fast path for regular symbols
        if (__builtin_expect(getCharLookup().hasFlag(c, CharLookup::SYMBOL_FLAG), 1)) {
            tokens.emplace_back(TokenType::SYMBOL, std::string_view(text + i, 1), i);
            i++;
            continue;
        }
        
        // Fast path for identifiers with optimized SIMD
        if (__builtin_expect(getCharLookup().hasFlag(c, CharLookup::IDENTIFIER_FLAG) && 
            !getCharLookup().hasFlag(c, CharLookup::DIGIT_FLAG), 1)) {
            size_t start = i++;
            size_t len = 1;
            
            // Use SIMD to find end of identifier when we have enough data
            while (i + 32 <= text_len) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
                
                // More efficient SIMD operations - using ranges for letters
                __m256i is_underscore = _mm256_cmpeq_epi8(chunk, underscore);
                
                // Optimized range checks
                __m256i is_digit = _mm256_and_si256(
                    _mm256_cmpgt_epi8(chunk, gt_zero),
                    _mm256_cmpgt_epi8(lt_nine, chunk)
                );
                
                __m256i is_lower = _mm256_and_si256(
                    _mm256_cmpgt_epi8(chunk, gt_a),
                    _mm256_cmpgt_epi8(lt_z, chunk)
                );
                
                __m256i is_upper = _mm256_and_si256(
                    _mm256_cmpgt_epi8(chunk, gt_A),
                    _mm256_cmpgt_epi8(lt_Z, chunk)
                );
                
                __m256i is_id_char = _mm256_or_si256(
                    _mm256_or_si256(is_underscore, is_digit),
                    _mm256_or_si256(is_lower, is_upper)
                );
                
                uint32_t id_bits = _mm256_movemask_epi8(is_id_char);
                
                if (__builtin_expect(id_bits == 0xFFFFFFFF, 1)) {
                    // All 32 bytes are identifier characters
                    i += 32;
                    len += 32;
                    continue;
                }
                
                // Find first non-identifier character
                unsigned int pos = __builtin_ctz(~id_bits);
                i += pos;
                len += pos;
                break;
            }
            
            // Process remaining bytes with optimized loop
            if (i < text_len) {
                const uint8_t* input = reinterpret_cast<const uint8_t*>(text + i);
                const uint8_t* end = reinterpret_cast<const uint8_t*>(text + text_len);
                while (input < end && getCharLookup().hasFlag(*input, CharLookup::IDENTIFIER_FLAG)) {
                    input++;
                }
                size_t remaining = input - reinterpret_cast<const uint8_t*>(text + i);
                i += remaining;
                len += remaining;
            }
            
            // Create token
            std::string_view token_view(text + start, len);
            
            // Fast keyword checking
            tokens.emplace_back(classify_keyword(token_view), token_view, start);

            continue;
        }
        
        // Fast path for numbers with SIMD (including negative numbers)
        if (__builtin_expect(getCharLookup().hasFlag(c, CharLookup::DIGIT_FLAG) || 
            (c == '-' && i + 1 < text_len && getCharLookup().hasFlag(text[i + 1], CharLookup::DIGIT_FLAG)), 0)) {
            size_t start = i;
            bool has_decimal = false;
            bool has_exponent = false;
            
            // Handle negative sign
            if (c == '-') {
                i++;
            }
            
            // Integer part - use SIMD for bulk scanning
            while (i + 32 <= text_len) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
                
                // Check for digits only (no decimals in this pass)
                __m256i is_digit = _mm256_and_si256(
                    _mm256_cmpgt_epi8(chunk, _mm256_set1_epi8('0' - 1)),
                    _mm256_cmpgt_epi8(_mm256_set1_epi8('9' + 1), chunk)
                );
                
                uint32_t digit_bits = _mm256_movemask_epi8(is_digit);
                
                if (digit_bits == 0xFFFFFFFF) {
                    i += 32;
                    continue;
                }
                
                // Found non-digit
                unsigned int pos = __builtin_ctz(~digit_bits);
                i += pos;
                break;
            }
            
            // Process remaining bytes (decimals, exponents)
            while (i < text_len) {
                c = text[i];
                if (c >= '0' && c <= '9') {
                    i++;
                } else if (c == '.' && !has_decimal && !has_exponent) {
                    // FIX: Ensure only one decimal point and not after exponent
                    has_decimal = true;
                    i++;
                } else if ((c == 'e' || c == 'E') && !has_exponent) {
                    // Scientific notation
                    has_exponent = true;
                    i++;
                    // Optional sign after exponent
                    if (i < text_len && (text[i] == '+' || text[i] == '-')) {
                        i++;
                    }
                } else {
                    break;
                }
            }
            
            tokens.emplace_back(TokenType::NUMBER, std::string_view(text + start, i - start), start);
            continue;
        }
        
        // Handle string literals (including block strings)
        if (__builtin_expect(getCharLookup().hasFlag(c, CharLookup::STRING_DELIM_FLAG), 0)) {
            char quote_char = c;
            size_t start = i;
            bool escaped = false;  // Declare at top to avoid goto issues
            
            // Check for block string (triple quotes)
            if (i + 2 < text_len && text[i + 1] == quote_char && text[i + 2] == quote_char) {
                i += 3;
                
                // Block string - scan for closing triple quotes
                while (i + 2 < text_len) {
                    if (text[i] == quote_char && text[i + 1] == quote_char && text[i + 2] == quote_char) {
                        i += 3;
                        tokens.emplace_back(TokenType::STRING, std::string_view(text + start, i - start), start);
                        goto string_done;
                    }
                    // Block strings can contain unescaped quotes and newlines
                    i++;
                }
                // ERROR: Unterminated block string
                tokens.emplace_back(TokenType::UNKNOWN, std::string_view(text + start, i - start), start);
                continue;
            }
            
            // Regular string - use improved escape tracking
            i++;
            
            // SIMD-accelerated scan for regular strings
            while (i + 32 <= text_len) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
                __m256i quote_v = _mm256_set1_epi8(quote_char);
                __m256i escape_v = _mm256_set1_epi8('\\');
                __m256i newline_v = _mm256_set1_epi8('\n');
                
                uint32_t quote_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, quote_v));
                uint32_t escape_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, escape_v));
                uint32_t newline_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, newline_v));
                
                // ERROR: Unescaped newline in string
                if (newline_mask) {
                    unsigned int nl_pos = __builtin_ctz(newline_mask);
                    i += nl_pos;
                    tokens.emplace_back(TokenType::UNKNOWN, std::string_view(text + start, i - start), start);
                    goto string_done;
                }
                
                // Look for unescaped quote
                if (quote_mask || escape_mask) {
                    // Fall back to scalar for this chunk (complex escape handling)
                    break;
                }
                
                i += 32;
            }
            
            // Scalar processing with proper escape tracking
            while (i < text_len) {
                char ch = text[i];
                
                if (escaped) {
                    escaped = false;
                    i++;
                    continue;
                }
                
                if (ch == '\\') {
                    escaped = true;
                    i++;
                } else if (ch == quote_char) {
                    i++; // Include closing quote
                    tokens.emplace_back(TokenType::STRING, std::string_view(text + start, i - start), start);
                    goto string_done;
                } else if (ch == '\n') {
                    // ERROR: Unterminated string (newline)
                    tokens.emplace_back(TokenType::UNKNOWN, std::string_view(text + start, i - start), start);
                    goto string_done;
                } else {
                    i++;
                }
            }
            
            // ERROR: Unterminated string (EOF)
            tokens.emplace_back(TokenType::UNKNOWN, std::string_view(text + start, i - start), start);
            string_done:
            continue;
        }
        
        // Handle unknown
        tokens.emplace_back(TokenType::UNKNOWN, std::string_view(text + i, 1), i);
        i++;
    }
    
    return tokens;
}
