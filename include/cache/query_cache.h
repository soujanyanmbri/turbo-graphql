#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>
#include "ast/ast_nodes.h"
#include "lexer/token/token.h"

// Cache entry for a parsed query
struct CacheEntry {
    std::unique_ptr<Document> ast;
    std::vector<Token> tokens;
    size_t access_count;
    std::chrono::steady_clock::time_point last_access;
    size_t memory_size;  // Approximate memory usage
    
    CacheEntry(std::unique_ptr<Document> ast_ptr, std::vector<Token> tok)
        : ast(std::move(ast_ptr))
        , tokens(std::move(tok))
        , access_count(1)
        , last_access(std::chrono::steady_clock::now())
        , memory_size(tokens.size() * sizeof(Token)) {}
};

// LRU cache for parsed GraphQL queries
class QueryCache {
public:
    explicit QueryCache(size_t max_size = 100, size_t max_memory_mb = 50);
    
    // Add a query to the cache
    void put(const std::string& query, 
             std::unique_ptr<Document> ast,
             const std::vector<Token>& tokens);
    
    // Get a cached query (returns nullptr if not found)
    const CacheEntry* get(const std::string& query);
    
    // Clear the cache
    void clear();
    
    // Get cache statistics
    struct Stats {
        size_t hits;
        size_t misses;
        size_t total_entries;
        size_t total_memory_bytes;
        double hit_rate;
    };
    Stats get_stats() const;
    
    // Enable/disable caching
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

private:
    size_t max_size_;
    size_t max_memory_bytes_;
    bool enabled_;
    
    std::unordered_map<std::string, std::unique_ptr<CacheEntry>> cache_;
    mutable std::mutex mutex_;
    
    // Statistics
    mutable size_t hits_;
    mutable size_t misses_;
    
    // Eviction policy: remove least recently used
    void evict_if_needed();
    std::string find_lru_key() const;
};

