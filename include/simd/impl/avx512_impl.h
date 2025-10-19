// #include "simd/simd_interface.h"

// // AVX512 fallback implementation (no SIMD)
// class AVX512TextProcessor : public TextProcessor {
// public:
//     size_t skipWhitespace(const char* text, size_t i, size_t textLen) const override;
//     size_t skipComments(const char* text, size_t i, size_t textLen) const override;
//     size_t findIdentifierEnd(const char* text, size_t start, size_t textLen) const override;
//     size_t findNumberEnd(const char* text, size_t start, size_t textLen, bool& hasDecimal) const override;
//     size_t findStringEnd(const char* text, size_t start, size_t textLen, char quoteChar) const override;


//     // These two are helpers and should not be marked override
//     size_t skipSingleLineComment(const char* text, size_t i, size_t textLen) const;
//     size_t skipMultiLineComment(const char* text, size_t i, size_t textLen) const;
// };