#pragma once

#include <cstddef>
#include "simd/simd_interface.h"

class AVX2TextProcessor : public SIMDInterface {
    public:
        size_t skipWhitespace(const char* text, size_t i, size_t textLen) const override;
        size_t skipComments(const char* text, size_t i, size_t textLen) const override;
        size_t findIdentifierEnd(const char* text, size_t i, size_t textLen) const override;
        size_t findNumberEnd(const char* text, size_t start, size_t text_len, bool& has_decimal) const override;
        size_t findStringEnd(const char* text, size_t start, size_t text_len, char quote_char) const override;
    
        // These two are helpers and should not be marked override
        size_t skipSingleLineComment(const char* text, size_t i, size_t textLen) const;
        size_t skipMultiLineComment(const char* text, size_t i, size_t textLen) const;
    };
    