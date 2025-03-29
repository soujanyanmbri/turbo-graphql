#ifndef SIMD_UTILS_H
#define SIMD_UTILS_H

#include "utils/SimdDetector.h"
#include <cstdint>
#include <cstddef>

using SIMDCharMaskFunc = int (*)(const char*, char);

class SimdUtils {
public:
    static void initialize();
    static int charMaskScalar(const char* data, char target);
// Adding this to public for now for granular testing
// private:
    static SIMDType activeSIMD;
    static SIMDCharMaskFunc charMask;

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    static int charMaskAVX512(const char* data, char target);
    static int charMaskAVX2(const char* data, char target);
    static int charMaskSSE(const char* data, char target);
    static int keywordMaskSSE(const char* data, size_t len, const char* keywords[], const size_t NUM_KEYWORDS);
    static int keywordMaskAVX2(const char* data, size_t len, const char* keywords[], const size_t NUM_KEYWORDS);
#endif
#if defined(__ARM_NEON) || defined(__aarch64__)
    static int charMaskNEON(const char* data, char target);
    static int keywordMaskNEON(const char* data, size_t len, const char keywords[][16], const size_t NUM_KEYWORDS);
#endif
};

#endif // SIMD_UTILS_H
