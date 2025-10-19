#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include "simd/simd_detect.h"
#include "lexer/lexer.h"
#include "lexer/token/token_arena.h"
#include "lexer/token/token.h"

// Helper function to convert TokenType to string for display
const char* tokenTypeToString(TokenType type) {
    switch (type) {
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
        case VARIABLE: return "VARIABLE";
        case DIRECTIVE: return "DIRECTIVE";
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
        case EXCLAMATION: return "EXCLAMATION";
        case BOOLEAN: return "BOOLEAN";
        case NULL_VALUE: return "NULL_VALUE";
        case UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

void printTokens(const std::pmr::vector<Token>& tokens, int max_tokens = 50) {
    std::cout << "\n+-----+---------------------+----------------------+-----------------+\n";
    std::cout << "| No. | Type                | Value                | Position        |\n";
    std::cout << "+-----+---------------------+----------------------+-----------------+\n";
    
    size_t limit = std::min(tokens.size(), (size_t)max_tokens);
    for (size_t i = 0; i < limit; i++) {
        const auto& token = tokens[i];
        std::string value(token.value);
        
        // Truncate long values
        if (value.length() > 20) {
            value = value.substr(0, 17) + "...";
        }
        
        // Escape newlines and tabs for display
        size_t pos = 0;
        while ((pos = value.find('\n', pos)) != std::string::npos) {
            value.replace(pos, 1, "\\n");
            pos += 2;
        }
        pos = 0;
        while ((pos = value.find('\t', pos)) != std::string::npos) {
            value.replace(pos, 1, "\\t");
            pos += 2;
        }
        
        std::cout << "| " << std::setw(3) << (i + 1) << " | "
                  << std::left << std::setw(19) << tokenTypeToString(token.type) << " | "
                  << std::setw(20) << value << " | "
                  << std::setw(15) << token.position << " |\n";
    }
    
    std::cout << "+-----+---------------------+----------------------+-----------------+\n";
    if (tokens.size() > max_tokens) {
        std::cout << "... and " << (tokens.size() - max_tokens) << " more tokens\n";
    }
    std::cout << "Total tokens: " << tokens.size() << "\n\n";
}

int main(int argc, char* argv[]) {
    // Print header
    std::cout << "\n====================================================================\n";
    std::cout << "         TURBO-GRAPHQL: High-Performance GraphQL Parser            \n";
    std::cout << "====================================================================\n\n";
    
    // Detect and display SIMD capabilities
    std::cout << "SIMD Detection: ";
    SIMDDetector::printBestSIMD();
    std::cout << std::endl;
    
    // Sample GraphQL query
    const char* sample_query = R"(
# Sample GraphQL Query
query GetUser($userId: ID!) {
    user(id: $userId) {
        name
        email
        age
        posts @include(if: true) {
            title
            content
            likes
        }
    }
}
)";
    
    // Check if a file was provided as argument
    std::string query_content;
    const char* query_to_parse = nullptr;
    size_t query_length = 0;
    
    if (argc > 1) {
        // Read from file
        std::ifstream file(argv[1]);
        if (file.good()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            query_content = buffer.str();
            query_to_parse = query_content.c_str();
            query_length = query_content.length();
            std::cout << "Parsing file: " << argv[1] << "\n";
        } else {
            std::cerr << "Error: Could not open file '" << argv[1] << "'\n";
            std::cerr << "Using sample query instead.\n\n";
            query_to_parse = sample_query;
            query_length = strlen(sample_query);
        }
    } else {
        query_to_parse = sample_query;
        query_length = strlen(sample_query);
        std::cout << "Using sample query (provide a .graphql file as argument to parse it)\n";
    }
    
    // Display the query
    std::cout << "\n" << std::string(68, '-') << "\n";
    std::cout << "Query to tokenize:\n";
    std::cout << std::string(68, '-') << "\n";
    std::cout << query_to_parse << "\n";
    std::cout << std::string(68, '-') << "\n";
    
    // Tokenize the query
    std::cout << "\nTokenizing with SIMD-accelerated lexer...\n";
    
    TokenArena arena;
    Tokenizer tokenizer;
    
    auto start = std::chrono::high_resolution_clock::now();
    auto& tokens = tokenizer.tokenize(query_to_parse, query_length, arena);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Tokenization completed in " << duration.count() << " microseconds\n";
    
    // Print the tokens
    printTokens(tokens);
    
    // Print statistics
    std::cout << "Statistics:\n";
    std::cout << "  - Input size: " << query_length << " bytes\n";
    std::cout << "  - Tokens generated: " << tokens.size() << "\n";
    std::cout << "  - Time taken: " << duration.count() << " microseconds\n";
    
    if (duration.count() > 0) {
        std::cout << "  - Throughput: " << std::fixed << std::setprecision(2) 
                  << (query_length / (duration.count() / 1000000.0) / 1024.0 / 1024.0) 
                  << " MB/s\n\n";
    }
    
    std::cout << "Example usage:\n";
    std::cout << "  ./build/graphql_parser                  # Parse sample query\n";
    std::cout << "  ./build/graphql_parser query.graphql   # Parse specific file\n\n";
    
    return 0;
}
