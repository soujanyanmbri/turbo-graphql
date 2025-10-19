#pragma once

#include <cstdint>

enum class SIMDType {
    AVX512,
    AVX2,
    SSE4_2,
    SSE2,
    NEON,  // For ARM
    SCALAR,
    UNKNOWN
};

class SIMDDetector {
public:
    static SIMDType detectBestSIMD();
    static void printBestSIMD();
};