#include <iostream>
#include <vector>
#include <cstring>
#include <immintrin.h>
#include <chrono>
#include <unordered_map>
#include <memory_resource>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iomanip>
// Increase buffer size for larger documents
constexpr size_t BUFFER_SIZE = 16'000 * sizeof(Token);

struct alignas(64) TokenArena {
    alignas(64) char buffer[BUFFER_SIZE];
    std::pmr::monotonic_buffer_resource resource{buffer, BUFFER_SIZE};
    std::pmr::vector<Token> tokens{&resource};
    
    void reset() {
        resource.release();
        tokens = std::pmr::vector<Token>(&resource);
    }
};

// Aligned lookup table for faster memory access
alignas(64) static uint8_t char_type_lut[256] = {0};
// New lookup table for specific token types
alignas(64) static TokenType special_char_lut[256] = {(TokenType)0};

// Character type bit flags
constexpr uint8_t WHITESPACE_FLAG = 1 << 0;
constexpr uint8_t DIGIT_FLAG      = 1 << 1;
constexpr uint8_t IDENTIFIER_FLAG = 1 << 2;
constexpr uint8_t SYMBOL_FLAG     = 1 << 3;
constexpr uint8_t STRING_DELIM_FLAG = 1 << 4;
constexpr uint8_t SPECIAL_CHAR_FLAG = 1 << 5;
constexpr uint8_t COMMENT_FLAG      = 1 << 6;  // New flag for comment characters

// Pre-compute mask for fast SIMD path
alignas(32) static const __m256i _whitespace_bytes = _mm256_setr_epi8(
    ' ', '\t', '\n', '\r', ' ', '\t', '\n', '\r',
    ' ', '\t', '\n', '\r', ' ', '\t', '\n', '\r',
    ' ', '\t', '\n', '\r', ' ', '\t', '\n', '\r',
    ' ', '\t', '\n', '\r', ' ', '\t', '\n', '\r'
);

    // Check for a-zA-Z0-9_
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

__attribute__((always_inline)) inline void initialize_char_class() {
    // Set all to 0
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
    
    // Set up special characters
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
    special_char_lut['{'] = LEFT_BRACE;
    special_char_lut['}'] = RIGHT_BRACE;
    special_char_lut['('] = LEFT_PAREN;
    special_char_lut[')'] = RIGHT_PAREN;
    special_char_lut['['] = LEFT_BRACKET;
    special_char_lut[']'] = RIGHT_BRACKET;
    special_char_lut[':'] = COLON;
    special_char_lut[','] = COMMA;
    special_char_lut['!'] = EXCLAMATION;
    
    // Other symbols - gather remaining GraphQL symbols
    const char symbols[] = "@!$<>#=+-*/&|^~%?";
    for (char c : symbols) {
        char_type_lut[(uint8_t)c] |= SYMBOL_FLAG;
    }
    
    // String delimiter
    char_type_lut['"'] |= STRING_DELIM_FLAG;
    char_type_lut['\''] |= STRING_DELIM_FLAG;  // Also allow single quotes for some GraphQL implementations
}

inline uint32_t calculate_keyword_hash(std::string_view sv) {
    const size_t len = sv.length();
    if (len < 2 || len > 11) return 0;
    
    // Use FNV-1a inspired hash algorithm
    uint32_t hash = 2166136261u; // FNV offset basis
    
    // Use first 3 chars and last char for better distribution
    hash ^= static_cast<uint32_t>(sv[0]);
    hash *= 16777619u; // FNV prime
    
    if (len > 1) {
        hash ^= static_cast<uint32_t>(sv[1]);
        hash *= 16777619u;
    }
    
    if (len > 2) {
        hash ^= static_cast<uint32_t>(sv[2]);
        hash *= 16777619u;
    }
    
    // Add the length as a feature
    hash ^= static_cast<uint32_t>(len);
    hash *= 16777619u;
    
    // Add last character if not already used
    if (len > 3) {
        hash ^= static_cast<uint32_t>(sv[len-1]);
        hash *= 16777619u;
    }
    
    return hash;
}

// Optimized keyword classifier with improved hash function
inline TokenType classify_keyword(std::string_view sv) {
    const size_t len = sv.length();
    if (len < 2 || len > 11) return TokenType::IDENTIFIER;
    
    uint32_t hash = calculate_keyword_hash(sv);
    
    switch (hash) {
        // 2-letter keywords
        case 0xd7274796: if (sv == "on") return TokenType::KEYWORD_ON; break;
        case 0xcfd041c6: if (sv == "id") return TokenType::KEYWORD_ID; break;
        
        // 3-letter keywords
        case 0x5b91ec67: if (sv == "int") return TokenType::KEYWORD_INT; break;
        
        // 4-letter keywords
        case 0x28191240: if (sv == "null") return TokenType::KEYWORD_NULL; break;
        case 0xf39f3875: if (sv == "true") return TokenType::KEYWORD_TRUE; break;
        case 0x0ecc1ead: if (sv == "type") return TokenType::KEYWORD_TYPE; break;
        case 0x301d0ad2: if (sv == "enum") return TokenType::KEYWORD_ENUM; break;
        
        // 5-letter keywords
        case 0x0f1ea48a: if (sv == "false") return TokenType::KEYWORD_FALSE; break;
        case 0xb71eb5f1: if (sv == "float") return TokenType::KEYWORD_FLOAT; break;
        case 0x0067774c: if (sv == "query") return TokenType::KEYWORD_QUERY; break;
        case 0xf722ba8d: if (sv == "__get") return TokenType::KEYWORD_GET; break;
        case 0xbc3d60fe: if (sv == "union") return TokenType::KEYWORD_UNION; break;
        case 0xf9601b2b: if (sv == "input") return TokenType::KEYWORD_INPUT; break;
        
        // 6-letter keywords
        case 0xdd26a99f: if (sv == "string") return TokenType::KEYWORD_STRING; break;
        case 0x7a8ff6e6: if (sv == "scalar") return TokenType::KEYWORD_SCALAR; break;
        
        // 7-letter keywords
        case 0xf1dd8306: if (sv == "boolean") return TokenType::KEYWORD_BOOLEAN; break;
        case 0xa79f840e: if (sv == "extends") return TokenType::KEYWORD_EXTEND; break;
        
        // 8-letter keywords
        case 0x3cf1ef96: if (sv == "__delete") return TokenType::KEYWORD_DELETE; break;
        case 0xf90d7d2b: if (sv == "__schema") return TokenType::KEYWORD_SCHEMA; break;
        case 0xae760f61: if (sv == "__update") return TokenType::KEYWORD_UPDATE; break;
        case 0x4ea2b387: if (sv == "__create") return TokenType::KEYWORD_CREATE; break;
        case 0x080283b7: if (sv == "mutation") return TokenType::KEYWORD_MUTATION; break;
        case 0xecafb154: if (sv == "fragment") return TokenType::KEYWORD_FRAGMENT; break;
        
        // 9-letter keywords
        case 0x44a7a8b0: if (sv == "interface") return TokenType::KEYWORD_INTERFACE; break;
        case 0x8e59d5e6: if (sv == "directive") return TokenType::KEYWORD_DIRECTIVE; break;
        
        // 10-letter keywords
        case 0xc9177ae6: if (sv == "implements") return TokenType::KEYWORD_IMPLEMENTS; break;
        case 0xf7614f98: if (sv == "__typename") return TokenType::KEYWORD_TYPENAME; break;
        
        // 11-letter keywords
        case 0x00000000: if (sv == "subscription") return TokenType::KEYWORD_SUBSCRIPTION; break;
    }
    return TokenType::IDENTIFIER;
}
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

          i += 31; // Overlap one byte in case '*' is at the boundary
      }

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
__attribute__((hot)) std::pmr::vector<Token>& tokenizeGraphQLWithSIMD(
    const char* __restrict__ text, 
    size_t text_len, 
    TokenArena& __restrict__ arena) {
    

    // Pre-allocate with exact size for small documents or a reasonable estimate for larger ones
    std::pmr::vector<Token>& tokens = arena.tokens;
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
                unsigned int skip = __builtin_ctz(~ws_bits | (ws_bits == 0xFFFFFFFF ? 0 : 0));
                i += skip;
                break;
            }
        } 
        
        // Handle remaining whitespace with LUT
        while (i < text_len && (char_type_lut[(uint8_t)text[i]] & WHITESPACE_FLAG)) {
            i++;
        }
        
        if (i >= text_len) break;
        
        char c = text[i];
        
        // Check for comments first (optimized with SIMD)

        // Fast path for comment detection (//, /*, or #)
        if (__builtin_expect((c == '#') || (c == '/' && i + 1 < text_len && 
        (text[i + 1] == '/' || text[i + 1] == '*')), 0)) {
                i = skip_comments_simd(text, i, text_len);
                continue;
        }

        
        // Check for ellipsis next
        if (i + 2 < text_len && *(const uint32_t*)(text + i) == 0x2E2E2E) {
            // 0x2E is ASCII for '.'
            // So 0x2E2E2E = '.', '.', '.'
            tokens.emplace_back(TokenType::ELLIPSIS, std::string_view(&text[i], 3), i);
            i += 3;
            continue;
        }
        
        if ((c == '$' || c == '@') && i + 1 < text_len && (char_type_lut[(uint8_t)text[i + 1]] & IDENTIFIER_FLAG)) {
            TokenType type = (c == '$') ? TokenType::VARIABLE : TokenType::DIRECTIVE;
            size_t start = i++;
            size_t len = 1;
        
            while (i < text_len && (char_type_lut[(uint8_t)text[i]] & IDENTIFIER_FLAG)) {
                i++;
                len++;
            }
        
            tokens.emplace_back(type, std::string_view(text + start, len), start);
            continue;
        }
        
        
        // Fast path for special characters
        if (__builtin_expect((char_type_lut[(uint8_t)c] & SPECIAL_CHAR_FLAG), 1)) {
            TokenType type = special_char_lut[(uint8_t)c];
            tokens.emplace_back(type, std::string_view(text + i, 1), i);
            i++;
            continue;
        }

        // Fast path for regular symbols
        if (__builtin_expect((char_type_lut[(uint8_t)c] & SYMBOL_FLAG), 1)) {
            tokens.emplace_back(TokenType::SYMBOL, std::string_view(text + i, 1), i);
            i++;
            continue;
        }
        
        // Fast path for identifiers with optimized SIMD
        if (__builtin_expect((char_type_lut[(uint8_t)c] & IDENTIFIER_FLAG) && !(char_type_lut[(uint8_t)c] & DIGIT_FLAG), 1)) {
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
                while (input < end && (char_type_lut[*input] & IDENTIFIER_FLAG)) {
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
        
        // Fast path for numbers with SIMD
        if (__builtin_expect((char_type_lut[(uint8_t)c] & DIGIT_FLAG), 0)) {
            size_t start = i++;
            size_t len = 1;
            bool has_decimal = false;
            
            // Special handling for numbers with SIMD
            while (i + 32 <= text_len) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
                
                // Check for digits and decimal point
                __m256i is_digit = _mm256_and_si256(
                    _mm256_cmpgt_epi8(chunk, _mm256_set1_epi8('0' - 1)),
                    _mm256_cmpgt_epi8(_mm256_set1_epi8('9' + 1), chunk)
                );
                
                // Handle decimal point separately to avoid multiple decimal points
                if (!has_decimal) {
                    __m256i is_decimal = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('.'));
                    uint32_t decimal_bits = _mm256_movemask_epi8(is_decimal);
                    if (decimal_bits != 0) {
                        unsigned int pos = __builtin_ctz(decimal_bits);
                        if (pos < 32) {
                            has_decimal = true;
                        }
                    }
                    is_digit = _mm256_or_si256(is_digit, is_decimal);
                }
                
                uint32_t digit_bits = _mm256_movemask_epi8(is_digit);
                
                if (__builtin_expect(digit_bits == 0xFFFFFFFF, 0)) {
                    // All 32 bytes are digits or one decimal point
                    i += 32;
                    len += 32;
                    continue;
                }
                
                // Find first non-digit
                unsigned int pos = __builtin_ctz(~digit_bits);
                i += pos;
                len += pos;
                break;
            }
            
            // Process remaining bytes
            while (i < text_len) {
                c = text[i];
                if (c >= '0' && c <= '9') {
                    i++;
                    len++;
                } else if (c == '.' && !has_decimal) {
                    has_decimal = true;
                    i++;
                    len++;
                } else {
                    break;
                }
            }
            
            tokens.emplace_back(TokenType::NUMBER, std::string_view(text + start, len), start);
            continue;
        }
        
        // Handle string literals
        if (__builtin_expect((char_type_lut[(uint8_t)c] & STRING_DELIM_FLAG), 0)) {
            char quote_char = c;
            size_t start = i++;
            
            // SIMD-accelerated string scan
            __m256i quote_v = _mm256_set1_epi8(quote_char);
            __m256i escape_v = _mm256_set1_epi8('\\');

            while (i + 32 <= text_len) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
                uint32_t quote_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, quote_v));
                uint32_t escape_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, escape_v));
                
                // Process escape sequences and quotes...
            }
            if (i < text_len) i++;  // Skip closing quote
            
            tokens.emplace_back(TokenType::STRING, std::string_view(text + start, i - start), start);
            continue;
        }
        
        tokens.emplace_back(TokenType::UNKNOWN, std::string_view(text + i, 1), i);
        i++;
    }
    
    return tokens;
}

// Original naive version for comparison - updated to handle special tokens and comments
std::vector<Token> tokenizeGraphQLNaive(const char* text, size_t text_len) {
    std::vector<Token> tokens;
    size_t i = 0;

    while (i < text_len) {
        char c = text[i];

        if (std::isspace(c)) {
            i++;
            continue;
        }
        
        // Handle comments
        if (c == '/' && i + 1 < text_len) {
            if (text[i + 1] == '/') {  // Single-line comment
                i += 2;
                while (i < text_len && text[i] != '\n') {
                    i++;
                }
                if (i < text_len) i++;  // Skip newline
                continue;
            } else if (text[i + 1] == '*') {  // Multi-line comment
                i += 2;
                while (i + 1 < text_len && !(text[i] == '*' && text[i + 1] == '/')) {
                    i++;
                }
                if (i + 1 < text_len) i += 2;  // Skip */
                continue;
            }
        }

        // Check for ellipsis
        if (c == '.' && i + 2 < text_len && text[i+1] == '.' && text[i+2] == '.') {
            tokens.emplace_back(TokenType::ELLIPSIS, std::string(text + i, 3), i);
            i += 3;
            continue;
        }

        // Check for special characters
        if (c == '{') {
            tokens.emplace_back(TokenType::LEFT_BRACE, std::string(1, c), i);
            i++;
            continue;
        } else if (c == '}') {
            tokens.emplace_back(TokenType::RIGHT_BRACE, std::string(1, c), i);
            i++;
            continue;
        } else if (c == '(') {
            tokens.emplace_back(TokenType::LEFT_PAREN, std::string(1, c), i);
            i++;
            continue;
        } else if (c == ')') {
            tokens.emplace_back(TokenType::RIGHT_PAREN, std::string(1, c), i);
            i++;
            continue;
        } else if (c == '[') {
            tokens.emplace_back(TokenType::LEFT_BRACKET, std::string(1, c), i);
            i++;
            continue;
        } else if (c == ']') {
            tokens.emplace_back(TokenType::RIGHT_BRACKET, std::string(1, c), i);
            i++;
            continue;
        } else if (c == ':') {
            tokens.emplace_back(TokenType::COLON, std::string(1, c), i);
            i++;
            continue;
        } else if (c == ',') {
            tokens.emplace_back(TokenType::COMMA, std::string(1, c), i);
            i++;
            continue;
        } else if (std::ispunct(c)) {
            tokens.emplace_back(TokenType::SYMBOL, std::string(1, c), i);
            i++;
            continue;
        }

        if (std::isdigit(c)) {
            size_t start = i;
            bool has_decimal = false;
            while (i < text_len) {
                if (std::isdigit(text[i])) {
                    i++;
                } else if (text[i] == '.' && !has_decimal) {
                    has_decimal = true;
                    i++;
                } else {
                    break;
                }
            }
            tokens.emplace_back(TokenType::NUMBER, std::string(text + start, i - start), start);
            continue;
        }

        if (std::isalpha(c) || c == '_') {
            size_t start = i;
            while (i < text_len && (std::isalnum(text[i]) || text[i] == '_')) i++;
            std::string token_value(text + start, i - start);
            TokenType token_type = classify_keyword(token_value);
            tokens.emplace_back(token_type, token_value, start);
            // auto it = keywordMap.find(token_value);
            // if (it != keywordMap.end()) {
            //     tokens.emplace_back(it->second, token_value, start);
            // } else {
            //     tokens.emplace_back(TokenType::IDENTIFIER, token_value, start);
            // }
            // continue;
        }

        tokens.emplace_back(TokenType::UNKNOWN, std::string(1, c), i);
        i++;
    }

    return tokens;
}

void benchmark_tokenizers(const char* input, size_t input_len, int iterations = 100) {
  TokenArena arena;
  std::vector<double> naive_times;
  std::vector<double> simd_times;

  // --- Warm-up run to stabilize cache/branch prediction ---
  for (int i = 0; i < 10; i++) {
      tokenizeGraphQLNaive(input, input_len);
      tokenizeGraphQLWithSIMD(input, input_len, arena);
  }

  // --- Benchmark Loop ---
  for (int i = 0; i < iterations; i++) {
      // Naive
      auto start_naive = std::chrono::high_resolution_clock::now();
      auto tokens_naive = tokenizeGraphQLNaive(input, input_len);
      auto end_naive = std::chrono::high_resolution_clock::now();
      naive_times.push_back(std::chrono::duration<double, std::micro>(end_naive - start_naive).count());

      // SIMD
      arena.reset();
      auto start_simd = std::chrono::high_resolution_clock::now();
      auto& tokens_simd = tokenizeGraphQLWithSIMD(input, input_len, arena);
      auto end_simd = std::chrono::high_resolution_clock::now();
      simd_times.push_back(std::chrono::duration<double, std::micro>(end_simd - start_simd).count());
  }

  auto stats = [](std::vector<double>& times) {
      std::sort(times.begin(), times.end());
      double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
      double med = times[times.size() / 2];
      double min = times.front();
      double max = times.back();
      double stdev = std::sqrt(std::accumulate(times.begin(), times.end(), 0.0, [&](double acc, double val) {
          return acc + (val - avg) * (val - avg);
      }) / times.size());

      return std::tuple{avg, med, min, max, stdev};
  };

  auto [avg_naive, med_naive, min_naive, max_naive, stdev_naive] = stats(naive_times);
  auto [avg_simd, med_simd, min_simd, max_simd, stdev_simd] = stats(simd_times);

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "\n📊 Benchmark Results (µs) over " << iterations << " iterations:\n";
  std::cout << "──────────────────────────────────────────────\n";
  std::cout << "Tokenizer   | Avg     | Median  | Min     | Max     | StdDev\n";
  std::cout << "------------|---------|---------|---------|---------|--------\n";
  std::cout << "Naive       | " << avg_naive << " | " << med_naive << " | " << min_naive
            << " | " << max_naive << " | " << stdev_naive << "\n";
  std::cout << "SIMD        | " << avg_simd << " | " << med_simd << " | " << min_simd
            << " | " << max_simd << " | " << stdev_simd << "\n";

  double speedup = avg_naive / avg_simd;
  std::cout << "\n🚀 Speedup: " << (speedup - 1.0) * 100.0 << "% faster (" << speedup << "x)\n";

  // Correctness check (optional)
  auto tokens_naive = tokenizeGraphQLNaive(input, input_len);
  auto& tokens_simd = tokenizeGraphQLWithSIMD(input, input_len, arena);
  bool matches = tokens_naive.size() == tokens_simd.size();

  if (!matches) {
      std::cout << "❌ Token count mismatch! naive=" << tokens_naive.size()
                << " simd=" << tokens_simd.size() << "\n";
  } else {
      for (size_t i = 0; i < tokens_naive.size(); i++) {
          if (tokens_naive[i].type != tokens_simd[i].type || tokens_naive[i].value != tokens_simd[i].value) {
              std::cout << "❌ Mismatch at token " << i << "\n";
              std::cout << "   Naive: " << tokens_naive[i].value << "\n";
              std::cout << "   SIMD : " << tokens_simd[i].value << "\n";
              matches = false;
              break;
          }
      }
  }
  std::cout << "✅ Output correctness: " << (matches ? "VERIFIED ✓" : "FAILED ✗") << "\n";
}


int main() {
    initialize_char_class();
    
    const char* gql_query = R"(
      # Top-level query for user and post stats
      query BenchmarkQuery($userId: ID!, $includeMeta: Boolean!, $filters: PostFilterInput, $limit: Int = 10) @benchmark {
        # Fetch user info
        user(id: $userId) {
          ...UserFields # Basic user fields
          posts(filter: $filters, limit: $limit) {
            edges {
              node {
                id
                title
                content
                tags
                createdAt
                metadata @include(if: $includeMeta) {
                  views
                  likes
                  shares
                }
                author {
                  ... on Admin {
                    privileges
                    accessLevel
                  }
                  ... on RegularUser {
                    reputation
                    joinedAt
                  }
                }
              }
            }
            pageInfo {
              hasNextPage
              endCursor
            }
          }
        }

        stats {
          totalUsers
          activeUsers
          postCounts {
            daily
            weekly
            monthly
          }
        }
      }

      # Reusable user fragment
      fragment UserFields on User {
        id
        name
        email
        role
        settings {
          theme
          notifications
        }
          test
      }

      )";
      
    
    TokenArena arena;
    
    // Run benchmark
    benchmark_tokenizers(gql_query, strlen(gql_query));
    
    // Print sample output with token type names
    std::cout << "\nFull token list from SIMD tokenizer:\n";
    auto& tokens = tokenizeGraphQLWithSIMD(gql_query, strlen(gql_query), arena);

    // Helper function to convert TokenType to string
    auto getTokenTypeName = [](TokenType type) -> std::string {
        switch(type) {
            case KEYWORD_QUERY: return "KEYWORD_QUERY";
            case KEYWORD_MUTATION: return "KEYWORD_MUTATION";
            case KEYWORD_SUBSCRIPTION: return "KEYWORD_SUBSCRIPTION";
            case KEYWORD_FRAGMENT: return "KEYWORD_FRAGMENT";
            case KEYWORD_ON: return "KEYWORD_ON";
            case KEYWORD_TRUE: return "KEYWORD_TRUE";
            case KEYWORD_FALSE: return "KEYWORD_FALSE";
            case KEYWORD_NULL: return "KEYWORD_NULL";
            case KEYWORD_TYPE: return "KEYWORD_TYPE";
            case KEYWORD_INPUT: return "KEYWORD_INPUT";
            case KEYWORD_ENUM: return "KEYWORD_ENUM";
            case KEYWORD_INTERFACE: return "KEYWORD_INTERFACE";
            case KEYWORD_UNION: return "KEYWORD_UNION";
            case KEYWORD_DIRECTIVE: return "KEYWORD_DIRECTIVE";
            case KEYWORD_SCALAR: return "KEYWORD_SCALAR";
            case KEYWORD_EXTEND: return "KEYWORD_EXTEND";
            case KEYWORD_IMPLEMENTS: return "KEYWORD_IMPLEMENTS";
            case KEYWORD_TYPENAME: return "KEYWORD_TYPENAME";
            case KEYWORD_SCHEMA: return "KEYWORD_SCHEMA";
            case KEYWORD_TYPE_META: return "KEYWORD_TYPE_META";
            case KEYWORD_GET: return "KEYWORD_GET";
            case KEYWORD_CREATE: return "KEYWORD_CREATE";
            case KEYWORD_UPDATE: return "KEYWORD_UPDATE";
            case KEYWORD_DELETE: return "KEYWORD_DELETE";
            case KEYWORD_INT: return "KEYWORD_INT";
            case KEYWORD_FLOAT: return "KEYWORD_FLOAT";
            case KEYWORD_STRING: return "KEYWORD_STRING";
            case KEYWORD_BOOLEAN: return "KEYWORD_BOOLEAN";
            case KEYWORD_ID: return "KEYWORD_ID";

            case IDENTIFIER: return "IDENTIFIER";
            case NUMBER: return "NUMBER";
            case VARIABLE: return "VARIABLE";
            case DIRECTIVE: return "DIRECTIVE";
            case STRING: return "STRING";
            case SYMBOL: return "SYMBOL";
            case LEFT_BRACE: return "LEFT_BRACE";
            case RIGHT_BRACE: return "RIGHT_BRACE";
            case EXCLAMATION: return "EXCLAMATION";
            case LEFT_PAREN: return "LEFT_PAREN";
            case RIGHT_PAREN: return "RIGHT_PAREN";
            case LEFT_BRACKET: return "LEFT_BRACKET";
            case RIGHT_BRACKET: return "RIGHT_BRACKET";
            case COLON: return "COLON";
            case COMMA: return "COMMA";
            case ELLIPSIS: return "ELLIPSIS";
            default: return "UNKNOWN";
        }
    };
    
    for (size_t i = 0; i < tokens.size(); i++) {
        std::cout << getTokenTypeName(tokens[i].type) << ": " << tokens[i].value << "\n";
    }

    std::cout << "\n FUll token list from naive tokenizer:\n";
    auto tokens_naive = tokenizeGraphQLNaive(gql_query, strlen(gql_query));
    
    for (size_t i = 0; i < tokens_naive.size(); i++) {
        std::cout << getTokenTypeName(tokens_naive[i].type) << ": " << &tokens_naive[i].value << "\n";
    }
    
    return 0;
}