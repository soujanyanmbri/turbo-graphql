#ifndef SIMD_UTILS_H
#define SIMD_UTILS_H

#include "utils/SimdDetector.h"
#include <cstdint>

using SIMDCharMaskFunc = int (*)(const char*, char);

class SimdUtils {
public:
    static void initialize();
    static int charMaskScalar(const char* data, char target);

private:
    static SIMDType activeSIMD;
    static SIMDCharMaskFunc charMask;

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    static int charMaskAVX512(const char* data, char target);
    static int charMaskAVX2(const char* data, char target);
    static int charMaskSSE(const char* data, char target);
#endif
};

#endif // SIMD_UTILS_H
