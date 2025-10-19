#include <memory_resource>
#include <vector>
#include "lexer/token/token_arena.h"

TokenArena::TokenArena(size_t buffer_size) 
    : buffer_size(buffer_size),
      buffer(new char[buffer_size]),
      resource(buffer, buffer_size),
      tokens_vector(&resource) {
}

TokenArena::~TokenArena() {
    delete[] buffer;
}

void TokenArena::reset() {
    resource.~monotonic_buffer_resource();
    new (&resource) std::pmr::monotonic_buffer_resource(buffer, buffer_size);
    
    // Reset the vector
    tokens_vector.clear();
}

std::pmr::vector<Token>& TokenArena::tokens() {
    return tokens_vector;
}

size_t TokenArena::getBufferSize() const {
    return buffer_size;
}

bool TokenArena::isExhausted() const {
    // Get a reference to the null memory resource
    std::pmr::memory_resource* null_resource = std::pmr::null_memory_resource();
    return resource.upstream_resource()->is_equal(*null_resource);
}