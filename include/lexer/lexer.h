#pragma once

#include <string_view>
#include <vector>
#include <memory_resource>

#include "lexer/token/token.h"
#include "lexer/token/token_arena.h"

class Tokenizer {
public:
    // Main tokenize function
    std::pmr::vector<Token>& tokenize(const char* text, 
                                      size_t text_len, 
                                      TokenArena& arena);
};
