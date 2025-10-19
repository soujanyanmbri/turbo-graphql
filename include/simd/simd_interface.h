#pragma once
#include <cstddef>

class SIMDInterface {
public:
    virtual ~SIMDInterface() = default;

    virtual size_t skipWhitespace(const char* text, size_t start, size_t text_len) const = 0;
    virtual size_t skipComments(const char* text, size_t start, size_t text_len) const = 0;
    virtual size_t findIdentifierEnd(const char* text, size_t start, size_t text_len) const = 0;
    virtual size_t findNumberEnd(const char* text, size_t start, size_t text_len, bool& has_decimal) const = 0;
    virtual size_t findStringEnd(const char* text, size_t start, size_t text_len, char quote_char) const = 0;

    static const SIMDInterface& getInstance();
};
