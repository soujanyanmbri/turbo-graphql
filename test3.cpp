#include <iostream>
#include <vector>
#include <cstring>
#include <immintrin.h>
#include <chrono>
#include <unordered_map>
#include <memory_resource>

enum TokenType {
    KEYWORD,
    IDENTIFIER,
    NUMBER,
    STRING,
    SYMBOL,

    // Special characters with dedicated token types
    LEFT_BRACE,      // {
    RIGHT_BRACE,     // }
    LEFT_PAREN,      // (
    RIGHT_PAREN,     // )
    LEFT_BRACKET,    // [
    RIGHT_BRACKET,   // ]
    COLON,           // :
    COMMA,           // ,
    ELLIPSIS,        // ...
    
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string_view value;
    size_t position;

    Token(TokenType t, std::string_view v, size_t p)
        : type(t), value(v), position(p) {}
};

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

// Pre-compute mask for fast SIMD path
alignas(32) static const __m256i _whitespace_bytes = _mm256_setr_epi8(
    ' ', '\t', '\n', '\r', ' ', '\t', '\n', '\r',
    ' ', '\t', '\n', '\r', ' ', '\t', '\n', '\r',
    ' ', '\t', '\n', '\r', ' ', '\t', '\n', '\r',
    ' ', '\t', '\n', '\r', ' ', '\t', '\n', '\r'
);

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
    
    // Set up special characters
    char_type_lut['{'] |= SPECIAL_CHAR_FLAG;
    char_type_lut['}'] |= SPECIAL_CHAR_FLAG;
    char_type_lut['('] |= SPECIAL_CHAR_FLAG;
    char_type_lut[')'] |= SPECIAL_CHAR_FLAG;
    char_type_lut['['] |= SPECIAL_CHAR_FLAG;
    char_type_lut[']'] |= SPECIAL_CHAR_FLAG;
    char_type_lut[':'] |= SPECIAL_CHAR_FLAG;
    char_type_lut[','] |= SPECIAL_CHAR_FLAG;
    
    // Map special chars to token types
    special_char_lut['{'] = LEFT_BRACE;
    special_char_lut['}'] = RIGHT_BRACE;
    special_char_lut['('] = LEFT_PAREN;
    special_char_lut[')'] = RIGHT_PAREN;
    special_char_lut['['] = LEFT_BRACKET;
    special_char_lut[']'] = RIGHT_BRACKET;
    special_char_lut[':'] = COLON;
    special_char_lut[','] = COMMA;
    
    // Other symbols - gather remaining GraphQL symbols
    const char symbols[] = "@!$<>#=+-*/&|^~%?";
    for (char c : symbols) {
        char_type_lut[(uint8_t)c] |= SYMBOL_FLAG;
    }
    
    // String delimiter
    char_type_lut['"'] |= STRING_DELIM_FLAG;
    char_type_lut['\''] |= STRING_DELIM_FLAG;  // Also allow single quotes for some GraphQL implementations
}

// Optimized keyword map with perfect hashing for common GraphQL keywords
static const std::unordered_map<std::string_view, TokenType> keywordMap = {
    {"query", KEYWORD}, {"mutation", KEYWORD}, {"fragment", KEYWORD}, {"on", KEYWORD},
    {"true", KEYWORD}, {"false", KEYWORD}, {"null", KEYWORD},
    {"int", KEYWORD}, {"float", KEYWORD}, {"string", KEYWORD}, {"boolean", KEYWORD},
    {"id", KEYWORD}, {"__typename", KEYWORD},
    {"__schema", KEYWORD}, {"__type", KEYWORD},
    {"__get", KEYWORD}, {"__create", KEYWORD}, {"__update", KEYWORD}, {"__delete", KEYWORD},
    {"interface", KEYWORD}, {"type", KEYWORD}, {"input", KEYWORD}, {"enum", KEYWORD},
    {"directive", KEYWORD}, {"scalar", KEYWORD}, {"extend", KEYWORD}, {"union", KEYWORD},
    {"implements", KEYWORD}, {"subscription", KEYWORD}
};

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

    // Check for ellipsis sequence
    auto checkEllipsis = [&](size_t pos) -> bool {
        if (pos + 2 < text_len && 
            text[pos] == '.' && 
            text[pos + 1] == '.' && 
            text[pos + 2] == '.') {
            tokens.emplace_back(TokenType::ELLIPSIS, std::string_view(&text[pos], 3), pos);
            return true;
        }
        return false;
    };
    
    // Process input in chunks of 32 bytes where possible
    while (i < text_len) {
        // Skip whitespace with SIMD when possible
        if (__builtin_expect(i + 32 <= text_len, 1)) {
            while (i + 32 <= text_len) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(text + i));
                
                // Generate masks for whitespace characters
                __m256i space_mask = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(' '));
                __m256i tab_mask = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\t'));
                __m256i nl_mask = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\n'));
                __m256i cr_mask = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\r'));
                
                // Combine masks
                __m256i whitespace_mask = _mm256_or_si256(
                    _mm256_or_si256(space_mask, tab_mask),
                    _mm256_or_si256(nl_mask, cr_mask)
                );
                
                // Convert to bitmask
                uint32_t ws_bits = _mm256_movemask_epi8(whitespace_mask);
                
                if (ws_bits == 0xFFFFFFFF) {
                    // All 32 bytes are whitespace
                    i += 32;
                    continue;
                }
                
                // Find first non-whitespace
                unsigned int pos = __builtin_ctz(~ws_bits);
                i += pos;
                break;
            }
        } 
        
        // Handle remaining whitespace with LUT
        while (i < text_len && (char_type_lut[(uint8_t)text[i]] & WHITESPACE_FLAG)) {
            i++;
        }
        
        if (i >= text_len) break;
        
        char c = text[i];
        
        // Check for ellipsis first
        if (c == '.' && checkEllipsis(i)) {
            i += 3;
            continue;
        }
        
        // Fast path for special characters
        if (__builtin_expect((char_type_lut[(uint8_t)c] & SPECIAL_CHAR_FLAG), 1)) {
            TokenType type = special_char_lut[(uint8_t)c];
            tokens.emplace_back(type, std::string_view(&text[i], 1), i);
            i++;
            continue;
        }
        
        // Fast path for regular symbols
        if (__builtin_expect((char_type_lut[(uint8_t)c] & SYMBOL_FLAG), 1)) {
            tokens.emplace_back(TokenType::SYMBOL, std::string_view(&text[i], 1), i);
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
                
                // Check for a-zA-Z0-9_
                __m256i underscore = _mm256_set1_epi8('_');
                __m256i zero = _mm256_set1_epi8('0');
                __m256i nine = _mm256_set1_epi8('9');
                
                // More efficient SIMD operations - using ranges for letters
                __m256i is_underscore = _mm256_cmpeq_epi8(chunk, underscore);
                
                // Optimized range checks
                __m256i is_digit = _mm256_and_si256(
                    _mm256_cmpgt_epi8(chunk, _mm256_set1_epi8('0' - 1)),
                    _mm256_cmpgt_epi8(_mm256_set1_epi8('9' + 1), chunk)
                );
                
                __m256i is_lower = _mm256_and_si256(
                    _mm256_cmpgt_epi8(chunk, _mm256_set1_epi8('a' - 1)),
                    _mm256_cmpgt_epi8(_mm256_set1_epi8('z' + 1), chunk)
                );
                
                __m256i is_upper = _mm256_and_si256(
                    _mm256_cmpgt_epi8(chunk, _mm256_set1_epi8('A' - 1)),
                    _mm256_cmpgt_epi8(_mm256_set1_epi8('Z' + 1), chunk)
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
            auto it = keywordMap.find(token_view);
            if (__builtin_expect(it != keywordMap.end(), 0)) {
                tokens.emplace_back(it->second, token_view, start);
            } else {
                tokens.emplace_back(TokenType::IDENTIFIER, token_view, start);
            }
            
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
            
            // Find closing quote
            while (i < text_len && text[i] != quote_char) {
                if (text[i] == '\\' && i + 1 < text_len) {
                    i += 2;  // Skip escape sequence
                } else {
                    i++;
                }
            }
            
            if (i < text_len) i++;  // Skip closing quote
            
            tokens.emplace_back(TokenType::STRING, std::string_view(text + start, i - start), start);
            continue;
        }
        
        // Unknown character
        tokens.emplace_back(TokenType::UNKNOWN, std::string_view(&text[i], 1), i);
        i++;
    }
    
    return tokens;
}

// Original naive version for comparison - updated to handle special tokens
std::vector<Token> tokenizeGraphQLNaive(const char* text, size_t text_len) {
    std::vector<Token> tokens;
    size_t i = 0;

    while (i < text_len) {
        char c = text[i];

        if (std::isspace(c)) {
            i++;
            continue;
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

// Benchmark function
void benchmark_tokenizers(const char* input, size_t input_len, int iterations = 100) {
    TokenArena arena;
    std::vector<double> naive_times;
    std::vector<double> simd_times;
    
    // Warm-up
    tokenizeGraphQLNaive(input, input_len);
    tokenizeGraphQLWithSIMD(input, input_len, arena);
    
    // Benchmark
    for (int i = 0; i < iterations; i++) {
        // Naive
        auto start_naive = std::chrono::high_resolution_clock::now();
        auto tokens_naive = tokenizeGraphQLNaive(input, input_len);
        auto end_naive = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed_naive = end_naive - start_naive;
        naive_times.push_back(elapsed_naive.count());
        
        // SIMD
        arena.reset();  // Reset the arena for reuse
        auto start_simd = std::chrono::high_resolution_clock::now();
        auto& tokens_simd = tokenizeGraphQLWithSIMD(input, input_len, arena);
        auto end_simd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed_simd = end_simd - start_simd;
        simd_times.push_back(elapsed_simd.count());
    }
    
    // Calculate statistics
    double naive_avg = 0, simd_avg = 0;
    for (int i = 0; i < iterations; i++) {
        naive_avg += naive_times[i];
        simd_avg += simd_times[i];
    }
    naive_avg /= iterations;
    simd_avg /= iterations;
    
    // Print results
    std::cout << "Benchmark results over " << iterations << " iterations:\n";
    std::cout << "Naïve Tokenizer Avg Time: " << naive_avg << " µs\n";
    std::cout << "Optimized SIMD Tokenizer Avg Time: " << simd_avg << " µs\n";
    
    // Percentage speedup
    double speedup = (naive_avg - simd_avg) / naive_avg;
    std::cout << "Average Speedup: " << speedup * 100 << "% (" << (naive_avg / simd_avg) << "x faster)\n";
    
    // Verify correctness
    auto tokens_naive = tokenizeGraphQLNaive(input, input_len);
    auto& tokens_simd = tokenizeGraphQLWithSIMD(input, input_len, arena);
    
    std::cout << "Tokens generated: naive=" << tokens_naive.size() 
              << ", SIMD=" << tokens_simd.size() << std::endl;
              
    bool matches = tokens_naive.size() == tokens_simd.size();
    
    // Find any differences in the output
    if (tokens_naive.size() != tokens_simd.size()) {
        std::cout << "Token count mismatch!\n";
    } else {
        for (size_t i = 0; i < tokens_naive.size() && i < tokens_simd.size(); i++) {
            if (tokens_naive[i].type != tokens_simd[i].type || 
                tokens_naive[i].value != tokens_simd[i].value) {
                matches = false;
                std::cout << "Mismatch at token " << i << ":\n";
                std::cout << "  Naive: " << tokens_naive[i].type << " - " << tokens_naive[i].value << "\n";
                std::cout << "  SIMD:  " << tokens_simd[i].type << " - " << tokens_simd[i].value << "\n";
                break;
            }
        }
    }
    
    std::cout << "Output correctness: " << (matches ? "VERIFIED ✓" : "MISMATCH ✗") << std::endl;
    
    // Output token type names for human readability
    auto tokenTypeName = [](TokenType type) -> std::string {
        switch(type) {
            case KEYWORD: return "KEYWORD";
            case IDENTIFIER: return "IDENTIFIER";
            case NUMBER: return "NUMBER";
            case STRING: return "STRING";
            case SYMBOL: return "SYMBOL";
            case LEFT_BRACE: return "LEFT_BRACE";
            case RIGHT_BRACE: return "RIGHT_BRACE";
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
    
    // Print first few tokens for verification
    std::cout << "\nSample tokens from SIMD tokenizer:\n";
    size_t max_tokens = std::min(tokens_simd.size(), size_t(20));
    for (size_t i = 0; i < max_tokens; i++) {
        std::cout << tokenTypeName(tokens_simd[i].type) << ": " << tokens_simd[i].value << "\n";
    }
}

int main() {
    initialize_char_class();
    
    const char* gql_query = R"(
{
  query: [
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
    
    TokenArena arena;
    
    // Run benchmark
    benchmark_tokenizers(gql_query, strlen(gql_query));
    
    // Print sample output with token type names
    std::cout << "\nFull token list from SIMD tokenizer:\n";
    auto& tokens = tokenizeGraphQLWithSIMD(gql_query, strlen(gql_query), arena);

    // Helper function to convert TokenType to string
    auto getTokenTypeName = [](TokenType type) -> std::string {
        switch(type) {
            case KEYWORD: return "KEYWORD";
            case IDENTIFIER: return "IDENTIFIER";
            case NUMBER: return "NUMBER";
            case STRING: return "STRING";
            case SYMBOL: return "SYMBOL";
            case LEFT_BRACE: return "LEFT_BRACE";
            case RIGHT_BRACE: return "RIGHT_BRACE";
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