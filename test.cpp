#include <iostream>
#include <chrono>
#include <string_view>
#include <vector>
#include <immintrin.h>  // AVX2 intrinsics
#include <bitset>

struct Token {
    enum class Type { Identifier, EndOfInput };
    Type type;
    std::string_view value;

    void print(bool truncate = true) const {
        if (type == Type::EndOfInput) {
            std::cout << "EndOfInput" << std::endl;
            return;
        }
        
        std::cout << "Identifier: ";
        if (truncate && value.size() > 30) {
            std::cout << value.substr(0, 30) << "... (length: " << value.size() << ")";
        } else {
            std::cout << value;
        }
        std::cout << std::endl;
    }
};

class NaiveLexer {
public:
    explicit NaiveLexer(std::string_view input) : input(input), position(0) {}

    Token processIdentifier() {
        // Check if we're already at the end
        if (position >= input.size()) 
            return { Token::Type::EndOfInput, "" };

        size_t start = position;
        size_t len = input.size();

        // Check if the first character is valid
        if (!((input[position] >= 'a' && input[position] <= 'z') ||
              (input[position] >= 'A' && input[position] <= 'Z') ||
              input[position] == '_')) {
            position++; // Move past invalid character
            return { Token::Type::EndOfInput, "" };
        }

        while (position < len && isIdentifierChar(input[position])) {
            position++;
        }

        return { Token::Type::Identifier, input.substr(start, position - start) };
    }

private:
    std::string_view input;
    size_t position;

    bool isIdentifierChar(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '_');
    }
};

class FixedGraphQLLexer {
public:
    explicit FixedGraphQLLexer(std::string_view input) : input(input), position(0) {}

    Token processIdentifier() {
        if (position >= input.size()) 
            return { Token::Type::EndOfInput, "" };

        size_t start = position;
        size_t len = input.size();

        // Check if the first character is valid
        if (!isAlpha(input[position]) && input[position] != '_') {
            position++;
            return { Token::Type::EndOfInput, "" };
        }

        // Process in 32-byte chunks using SIMD
        while (position + 32 <= len) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&input[position]));

            // Define masks for identifier characters
            __m256i isAlphaLower = _mm256_and_si256(
                _mm256_cmpgt_epi8(chunk, _mm256_set1_epi8((char)('a' - 1))), 
                _mm256_cmpgt_epi8(_mm256_set1_epi8((char)('z' + 1)), chunk)  
            );

            __m256i isAlphaUpper = _mm256_and_si256(
                _mm256_cmpgt_epi8(chunk, _mm256_set1_epi8((char)('A' - 1))), 
                _mm256_cmpgt_epi8(_mm256_set1_epi8((char)('Z' + 1)), chunk)  
            );

            __m256i isNum = _mm256_and_si256(
                _mm256_cmpgt_epi8(chunk, _mm256_set1_epi8((char)('0' - 1))), 
                _mm256_cmpgt_epi8(_mm256_set1_epi8((char)('9' + 1)), chunk)
            );

            __m256i isUnderscore = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('_'));

            // Combine valid masks
            __m256i valid = _mm256_or_si256(_mm256_or_si256(isAlphaLower, isAlphaUpper), 
                                            _mm256_or_si256(isNum, isUnderscore));

            uint32_t mask32 = static_cast<uint32_t>(_mm256_movemask_epi8(valid));

            if (mask32 == 0xFFFFFFFF) {
                position += 32;
                continue;
            }

            int firstInvalid = __builtin_ctz(~mask32);
            position += firstInvalid;
            break;

        }

        // Process any remaining characters traditionally
        while (position < len && isIdentifierChar(input[position])) {
            position++;
        }


        return { Token::Type::Identifier, input.substr(start, position - start) };
    }


private:
    std::string_view input;
    size_t position;

    bool isIdentifierChar(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '_');
    }

    bool isAlpha(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }
};

// Benchmark Function
template <typename Lexer>
void benchmarkLexer(const std::string& testInput, const std::string& lexerName) {
    Lexer lexer(testInput);
    Token token;
    size_t tokenCount = 0;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    do {
        token = lexer.processIdentifier();
        if (token.type == Token::Type::Identifier) {
            tokenCount++;
            token.print();
        }
    } while (token.type != Token::Type::EndOfInput);
    
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> duration = end - start;
    // Print out the token
    std::cout << lexerName << " Time: " << duration.count() << " ms (Tokens: " << tokenCount << ")\n";
}

int main() {
    // std::string testInput(10'000'000, 'a');  // 10M 'a' characters (long identifier)

    // Another string:
    // Another string:
    std::string testInput = "queryHeroNameAndFriendssdfjaeklfjasdfjasdkjflasd kfaklsd fsdklajflkasdjfklasdj lfasdjfkldasjflkasdj lfjasklfj asdlfjs ";

    std::cout << "Benchmarking GraphQL Lexer (Naive vs SIMD)\n";
    benchmarkLexer<NaiveLexer>(testInput, "Naive Lexer");
    benchmarkLexer<FixedGraphQLLexer>(testInput, "SIMD Lexer");
    
    return 0;
}