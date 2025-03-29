#ifndef SIMD_DETECTOR_H
#define SIMD_DETECTOR_H

#include <cstdint>

enum class SIMDType {
    AVX512,
    AVX2,
    SSE4_2,
    NEON,  // For ARM
    SCALAR,
    UNKNOWN
};

class SIMDDetector {
public:
    static SIMDType detectBestSIMD();
};

#endif // SIMD_DETECTOR_H
