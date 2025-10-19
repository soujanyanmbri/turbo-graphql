// TokenArena.h
#pragma once
#include <memory_resource>
#include <vector>
#include "token.h"


class TokenArena {
public:
    static constexpr size_t DEFAULT_BUFFER_SIZE = 16'000 * sizeof(Token);
    
    // Constructor with customizable buffer size
    explicit TokenArena(size_t buffer_size = DEFAULT_BUFFER_SIZE);
    
    ~TokenArena();
    
    // No copy or move
    TokenArena(const TokenArena&) = delete;
    TokenArena& operator=(const TokenArena&) = delete;
    TokenArena(TokenArena&&) = delete;
    TokenArena& operator=(TokenArena&&) = delete;
    
    // Reset the arena for reuse
    void reset();
    
    // Get access to the tokens vector
    std::pmr::vector<Token>& tokens();
    
    // Get buffer size
    size_t getBufferSize() const;
    
    // Check if arena is exhausted
    bool isExhausted() const;

    std::pmr::vector<Token> tokens_vector;

private:
    char* buffer;
    size_t buffer_size;
    std::pmr::monotonic_buffer_resource resource;
};
    
    