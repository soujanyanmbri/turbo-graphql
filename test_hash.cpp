#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <unordered_map>

// FNV-1a inspired hash function
uint32_t calculate_keyword_hash(const std::string& word) {
    const size_t len = word.length();
    if (len < 2 || len > 11) return 0;
    
    // Use FNV-1a inspired hash algorithm
    uint32_t hash = 2166136261u; // FNV offset basis
    
    // Use first 3 chars and last char for better distribution
    hash ^= static_cast<uint32_t>(word[0]);
    hash *= 16777619u; // FNV prime
    
    if (len > 1) {
        hash ^= static_cast<uint32_t>(word[1]);
        hash *= 16777619u;
    }
    
    if (len > 2) {
        hash ^= static_cast<uint32_t>(word[2]);
        hash *= 16777619u;
    }
    
    // Add the length as a feature
    hash ^= static_cast<uint32_t>(len);
    hash *= 16777619u;
    
    // Add last character if not already used
    if (len > 3) {
        hash ^= static_cast<uint32_t>(word[len-1]);
        hash *= 16777619u;
    }
    
    return hash;
}

int main() {
    // List of all keywords to hash
    const std::vector<std::string> keywords = {
        // 2-letter keywords
        "on", "id",
        
        // 3-letter keywords
        "int",
        
        // 4-letter keywords
        "null", "true", "type", "enum",
        
        // 5-letter keywords
        "false", "float", "query", "__get", "union", "input",
        
        // 6-letter keywords
        "string", "scalar",
        
        // 7-letter keywords
        "boolean", "extends",
        
        // 8-letter keywords
        "__delete", "__schema", "__update", "__create", "mutation", "fragment",
        
        // 9-letter keywords
        "interface", "directive",
        
        // 10-letter keywords
        "implements", "__typename",
        
        // 11-letter keywords
        "subscription"
    };
    
    // Store hashes for collision detection
    std::unordered_map<uint32_t, std::string> hash_map;
    
    // Print header
    std::cout << "Improved Keyword Hash Values:\n";
    std::cout << "===========================\n";
    std::cout << std::left << std::setw(15) << "Keyword" << " | " 
              << std::right << std::setw(12) << "Hash (Decimal)" << " | " 
              << "Hash (Hex)" << " | " 
              << "Length" << std::endl;
    std::cout << "-----------------------------------------------\n";
    
    // Calculate and print hash for each keyword
    for (const auto& word : keywords) {
        uint32_t hash = calculate_keyword_hash(word);
        std::cout << std::left << std::setw(15) << word << " | " 
                  << std::right << std::setw(12) << hash << " | " 
                  << "0x" << std::hex << std::setw(8) << std::setfill('0') << hash << std::setfill(' ') << " | " 
                  << std::dec << word.length() << std::endl;
                  
        // Check for collision
        if (hash_map.find(hash) != hash_map.end()) {
            std::cout << "COLLISION: \"" << word << "\" and \"" 
                      << hash_map[hash] << "\" have the same hash: 0x" 
                      << std::hex << hash << std::dec << std::endl;
        } else {
            hash_map[hash] = word;
        }
    }
    
    return 0;
}