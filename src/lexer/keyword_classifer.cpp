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


#include "lexer/token/token_type.h"
#include "lexer/keyword_classifier.h"

uint32_t calculate_keyword_hash(std::string_view sv) {
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
TokenType classify_keyword(std::string_view sv) {
    const size_t len = sv.length();
    if (len < 2 || len > 11) return TokenType::IDENTIFIER;
    
    uint32_t hash = calculate_keyword_hash(sv);
    
    switch (hash) {
        // 2-letter keywords
        case 0xd7274796: if (sv == "on") return TokenType::KEYWORD_ON; break;
        // Removed: case 0xcfd041c6: if (sv == "id") return TokenType::KEYWORD_ID; break;
        // "id" should be treated as regular identifier, not keyword
        
        // 3-letter keywords
        // Removed: case 0x5b91ec67: if (sv == "int") return TokenType::KEYWORD_INT; break;
        // "int" should be treated as regular identifier
        
        // 4-letter keywords
        case 0x28191240: if (sv == "null") return TokenType::KEYWORD_NULL; break;
        case 0xf39f3875: if (sv == "true") return TokenType::KEYWORD_TRUE; break;
        case 0x0ecc1ead: if (sv == "type") return TokenType::KEYWORD_TYPE; break;
        case 0x301d0ad2: if (sv == "enum") return TokenType::KEYWORD_ENUM; break;
        
        // 5-letter keywords
        case 0x0f1ea48a: if (sv == "false") return TokenType::KEYWORD_FALSE; break;
        // Removed: case 0xb71eb5f1: if (sv == "float") return TokenType::KEYWORD_FLOAT; break;
        case 0x0067774c: if (sv == "query") return TokenType::KEYWORD_QUERY; break;
        case 0xf722ba8d: if (sv == "__get") return TokenType::KEYWORD_GET; break;
        case 0xbc3d60fe: if (sv == "union") return TokenType::KEYWORD_UNION; break;
        case 0xf9601b2b: if (sv == "input") return TokenType::KEYWORD_INPUT; break;
        
        // 6-letter keywords
        // Removed: case 0xdd26a99f: if (sv == "string") return TokenType::KEYWORD_STRING; break;
        case 0x7a8ff6e6: if (sv == "scalar") return TokenType::KEYWORD_SCALAR; break;
        
        // 7-letter keywords
        // Removed: case 0xf1dd8306: if (sv == "boolean") return TokenType::KEYWORD_BOOLEAN; break;
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