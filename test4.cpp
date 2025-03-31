#include <iostream>
#include <vector>
#include <cstring>
#include <immintrin.h>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <memory_resource>  // C++17: PMR (Polymorphic Memory Resource)



enum TokenType {
    KEYWORD,
    IDENTIFIER,
    NUMBER,
    STRING,
    SYMBOL,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string_view value;
    size_t position;

    Token(TokenType t, std::string_view v, size_t p)
        : type(t), value(v), position(p) {}
};

// Define a memory buffer size (adjust as needed)
constexpr size_t BUFFER_SIZE = 10'000 * sizeof(Token);

// Bump Allocator with std::pmr::monotonic_buffer_resource
struct TokenArena {
    alignas(Token) char buffer[BUFFER_SIZE];  // Pre-allocated memory
    std::pmr::monotonic_buffer_resource resource{buffer, BUFFER_SIZE};  // Bump allocator
    std::pmr::vector<Token> tokens{&resource};  // Uses fast bump allocation
};

alignas(32) static uint8_t char_type_lut[256] = {0};


// bool is_whitespace = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
// bool is_digit = (c - '0' < 10);
// bool is_letter = ((c | 32) - 'a' < 26);

// Character type bit flags
constexpr uint8_t WHITESPACE_FLAG = 1 << 0;
constexpr uint8_t DIGIT_FLAG    = 1 << 1;
constexpr uint8_t IDENTIFIER_FLAG = 1 << 2;
constexpr uint8_t SYMBOL_FLAG     = 1 << 3;
constexpr uint8_t STRING_DELIM_FLAG = 1 << 4;

void initialize_char_class() {
    // Set all to 0
    std::memset(char_type_lut, 0, sizeof(char_type_lut));
    
    // Whitespace
    char_type_lut[' ']  |= WHITESPACE_FLAG;
    char_type_lut['\t'] |= WHITESPACE_FLAG;
    char_type_lut['\n'] |= WHITESPACE_FLAG;
    char_type_lut['\r'] |= WHITESPACE_FLAG;
    
    // Digits
    for (char c = '0'; c <= '9'; c++) {
        char_type_lut[(uint8_t)c] |= DIGIT_FLAG | IDENTIFIER_FLAG;
    }
    
    // Identifiers (a-z, A-Z, _)
    for (char c = 'a'; c <= 'z'; c++) {
        char_type_lut[(uint8_t)c] |= IDENTIFIER_FLAG;
    }
    for (char c = 'A'; c <= 'Z'; c++) {
        char_type_lut[(uint8_t)c] |= IDENTIFIER_FLAG;
    }
    char_type_lut['_'] |= IDENTIFIER_FLAG;
    
    // Symbols
    const char symbols[] = "{}()[]:,@!";
    for (char c : symbols) {
        char_type_lut[(uint8_t)c] |= SYMBOL_FLAG;
    }
    
    // String delimiter
    char_type_lut['"'] |= STRING_DELIM_FLAG;
}

// **2. Hash Table for Keywords (SIMD-Optimized)**
static const std::unordered_map<std::string, TokenType> keywordMap = {
    {"query", KEYWORD}, {"mutation", KEYWORD}, {"fragment", KEYWORD}, {"on", KEYWORD},
    {"true", KEYWORD}, {"false", KEYWORD}, {"null", KEYWORD},
    {"int", KEYWORD}, {"float", KEYWORD}, {"string", KEYWORD}, {"boolean", KEYWORD},
    {"id", KEYWORD}, {"__typename", KEYWORD},
    {"__schema", KEYWORD}, {"__type", KEYWORD},
    {"__get", KEYWORD}, {"__create", KEYWORD}, {"__update", KEYWORD}, {"__delete", KEYWORD},
};


std::pmr::vector<Token> tokenizeGraphQLWithSIMD(const char* text, size_t text_len, TokenArena& arena) {
    // Pre-allocate with exact size for small documents or a reasonable estimate for larger ones
    // This completely eliminates reallocations in most cases
    std::pmr::vector<Token>& tokens = arena.tokens;
    tokens.reserve(text_len > 1000 ? text_len / 3 : text_len);
    
    size_t i = 0;
    
    // Use a local buffer to reduce string allocation costs
    constexpr size_t MAX_TOKEN_SIZE = 256;
    char token_buffer[MAX_TOKEN_SIZE];
    
    // Process in chunks for better cache locality
    constexpr size_t CHUNK_SIZE = 1024;
    
    while (i < text_len) {
        // Fast skip all whitespace
        while (i < text_len && (char_type_lut[(uint8_t)text[i]] & WHITESPACE_FLAG)) {
            i++;
        }

        
        if (i >= text_len) break;
        
        char c = text[i];
        
        // Fast path for symbols - most common in GraphQL
        // static const char symbols[] = "{}()[]:,@!";
        // bool is_symbol = false;
        // for (char s : symbols) {
        //     if (c == s) {
        //         is_symbol = true;
        //         break;
        //     }
        // }
        
        // if (is_symbol) {
        //     // Use static TokenType for common symbols
        //     static const TokenType symbol_types[] = {
        //         TokenType::SYMBOL, // '{'
        //         TokenType::SYMBOL, // '}'
        //         TokenType::SYMBOL, // '('
        //         TokenType::SYMBOL, // ')'
        //         TokenType::SYMBOL, // '['
        //         TokenType::SYMBOL, // ']'
        //         TokenType::SYMBOL, // ':'
        //         TokenType::SYMBOL, // ','
        //         TokenType::SYMBOL, // '@'
        //         TokenType::SYMBOL  // '!'
        //     };
            
        //     token_buffer[0] = c;
        //     // For tokens of length 1, directly create Token with the character
        //     // to avoid allocating a std::string
        //     tokens.emplace_back(TokenType::SYMBOL, std::string_view(&c, 1), i);
        //     i++;
        //     continue;
        // }

        if (char_type_lut[(uint8_t)c] & SYMBOL_FLAG) {
            tokens.emplace_back(TokenType::SYMBOL, std::string_view(&text[i], 1), i);
            i++;  // Increment separately
            continue;
        }


        
        // Fast path for identifiers (including keywords)
        if (char_type_lut[(uint8_t)c] & IDENTIFIER_FLAG) {
            size_t start = i++;
            size_t len = 1;
            
            // Use SIMD to find end of identifier
            while (i + 32 <= text_len) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
                
                // Check for a-z, A-Z, 0-9, _
                __m256i underscore = _mm256_set1_epi8('_');
                __m256i zero = _mm256_set1_epi8('0');
                __m256i nine = _mm256_set1_epi8('9');
                __m256i lower_a = _mm256_set1_epi8('a');
                __m256i lower_z = _mm256_set1_epi8('z');
                __m256i upper_a = _mm256_set1_epi8('A');
                __m256i upper_z = _mm256_set1_epi8('Z');
                
                __m256i is_underscore = _mm256_cmpeq_epi8(chunk, underscore);
                __m256i is_digit = _mm256_and_si256(
                    _mm256_cmpgt_epi8(_mm256_add_epi8(chunk, _mm256_set1_epi8(1)), zero),
                    _mm256_cmpgt_epi8(_mm256_add_epi8(nine, _mm256_set1_epi8(1)), chunk)
                );
                __m256i is_lower = _mm256_and_si256(
                    _mm256_cmpgt_epi8(_mm256_add_epi8(chunk, _mm256_set1_epi8(1)), lower_a),
                    _mm256_cmpgt_epi8(_mm256_add_epi8(lower_z, _mm256_set1_epi8(1)), chunk)
                );
                __m256i is_upper = _mm256_and_si256(
                    _mm256_cmpgt_epi8(_mm256_add_epi8(chunk, _mm256_set1_epi8(1)), upper_a),
                    _mm256_cmpgt_epi8(_mm256_add_epi8(upper_z, _mm256_set1_epi8(1)), chunk)
                );
                
                __m256i is_id_char = _mm256_or_si256(
                    _mm256_or_si256(is_underscore, is_digit),
                    _mm256_or_si256(is_lower, is_upper)
                );
                
                uint32_t id_bits = _mm256_movemask_epi8(is_id_char);
                
                if (id_bits == 0xFFFF) {
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
            
            // Process remaining bytes
            while (i < text_len) {
                c = text[i];
                if (!(char_type_lut[(uint8_t)c] & IDENTIFIER_FLAG)) break;
                i++;
                len++;
            }
            
            // Copy token to buffer (safer than creating from pointer range)
            size_t copy_len = std::min(len, MAX_TOKEN_SIZE - 1);
            std::memcpy(token_buffer, text + start, copy_len);
            token_buffer[copy_len] = '\0';
            
            // Use string_view for lookup, then construct the final string only once
            std::string_view token_view(text + start, copy_len);
            
            // Fast keyword checking with unordered_map
            auto it = keywordMap.find(std::string(token_view));
            if (it != keywordMap.end()) {
                tokens.emplace_back(it->second,token_view, start);
            } else {
                tokens.emplace_back(TokenType::IDENTIFIER, token_view, start);
            }
            
            continue;
        }
        
        // Fast path for numbers
        if (char_type_lut[(uint8_t)c] & DIGIT_FLAG) {
            size_t start = i++;
            size_t len = 1;
            
            // Use SIMD to find end of number
            while (i + 32 <= text_len) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
                
                // Check for digits
                __m256i zero = _mm256_set1_epi8('0');
                __m256i nine = _mm256_set1_epi8('9');
                
                __m256i is_digit = _mm256_and_si256(
                    _mm256_cmpgt_epi8(_mm256_add_epi8(chunk, _mm256_set1_epi8(1)), zero),
                    _mm256_cmpgt_epi8(_mm256_add_epi8(nine, _mm256_set1_epi8(1)), chunk)
                );
                
                uint32_t digit_bits = _mm256_movemask_epi8(is_digit);
                
                if (digit_bits == 0xFFFF) {
                    // All 32 bytes are digits
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
            while (i < text_len && text[i] >= '0' && text[i] <= '9') {
                i++;
                len++;
            }
            
            // Copy number to buffer
            size_t copy_len = std::min(len, MAX_TOKEN_SIZE - 1);
            std::memcpy(token_buffer, text + start, copy_len);
            token_buffer[copy_len] = '\0';
            
            tokens.emplace_back(TokenType::NUMBER, std::string_view(text + start, copy_len), start);
            continue;
        }
        
        // Handle string literals
        if (c == '"') {
            size_t start = i++;
            
            // Find the closing quote
            while (i < text_len && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < text_len) i += 2;
                else i++;
            }
            
            if (i < text_len) i++; // Skip closing quote
            
            size_t len = i - start;
            size_t copy_len = std::min(len, MAX_TOKEN_SIZE - 1);
            std::memcpy(token_buffer, text + start, copy_len);
            token_buffer[copy_len] = '\0';
            
            tokens.emplace_back(TokenType::STRING, std::string_view(text + start, copy_len), start);
            continue;
        }
        
        // Skip unknown characters
        i++;
    }
    
    return tokens;
}
std::vector<Token> tokenizeGraphQLNaive(const char* text, size_t text_len) {
    std::vector<Token> tokens;
    size_t i = 0;

    while (i < text_len) {
        char c = text[i];

        if (std::isspace(c)) {
            i++;
            continue;
        }

        if (std::ispunct(c)) { 
            tokens.emplace_back(TokenType::SYMBOL, std::string(1, c), i);
            i++;
            continue;
        }

        if (std::isdigit(c)) {
            size_t start = i;
            while (i < text_len && std::isdigit(text[i])) i++;
            tokens.emplace_back(TokenType::NUMBER, std::string(text + start, i - start), start);
            continue;
        }

        if (std::isalpha(c) || c == '_') {
            size_t start = i;
            while (i < text_len && (std::isalnum(text[i]) || text[i] == '_')) i++;
            std::string token_value(text + start, i - start);

            auto it = keywordMap.find(token_value);
            if (it != keywordMap.end()) {
                tokens.emplace_back(it->second, token_value, start);
            } else {
                tokens.emplace_back(TokenType::IDENTIFIER, token_value, start);
            }
            continue;
        }

        tokens.emplace_back(TokenType::UNKNOWN, std::string(1, c), i);
        i++;
    }

    return tokens;
}


// **5. Benchmark**
int main() {
    initialize_char_class();
    TokenArena arena;  // Create a pre-allocated token buffer
    const char* gql_query = R"(
{
  "definitions": [
    {
      "operation": "mutation",
      "selectionSet": {
        "selections": [
          {
            "name": {
              "value": "fetchFact"
            },
            "arguments": [
              {
                "name": {
                  "value": "input"
                },
                "value": {
                  "fields": [
                    {
                      "name": {
                        "value": "student"
                      },
                      "value": {
                        "value": "Jacob"
                      }
                    },
                    {
                      "name": {
                        "value": "id"
                      },
                      "value": {
                        "value": "123"
                      }
                    }
                  ]
                }
              }
            ],
            "selectionSet": {
              "selections": [
                {
                  "name": {
                    "value": "fact"
                  }
                },
                {
                  "name": {
                    "value": "random"
                  }
                },
                {
                  "name": {
                    "value": "id"
                  }
                }
              ]
            }
          }
        ]
      }
    }
  ]
}
    )";

    // Benchmark Naïve
    auto start_naive = std::chrono::high_resolution_clock::now();
    auto tokens_naive = tokenizeGraphQLNaive(gql_query, strlen(gql_query));
    auto end_naive = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed_naive = end_naive - start_naive;

    // Benchmark SIMD
    auto start_simd = std::chrono::high_resolution_clock::now();
    auto tokens_simd = tokenizeGraphQLWithSIMD(gql_query, strlen(gql_query), arena);

    auto end_simd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed_simd = end_simd - start_simd;

    for (size_t i = 0; i < tokens_simd.size(); i++) {
        std::cout << tokens_simd[i].type << ": " << tokens_simd[i].value << "\n";
    }

  

    // Print results
    std::cout << "Naïve Tokenizer Time: " << elapsed_naive.count() << " µs\n";
    std::cout << "Optimized SIMD Tokenizer Time: " << elapsed_simd.count() << " µs\n";

    // Percentage speedup
    double speedup = (elapsed_naive.count() - elapsed_simd.count()) / elapsed_naive.count();
    std::cout << "Speedup: " << speedup * 100 << "%\n";

    return 0;
}
