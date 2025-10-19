#pragma once

#include <string_view>
#include "lexer/token/token_type.h"

// Calculate hash for keyword classification
uint32_t calculate_keyword_hash(std::string_view sv);

// Classify keyword or return IDENTIFIER if not a keyword
TokenType classify_keyword(std::string_view sv);

