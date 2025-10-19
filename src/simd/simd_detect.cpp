#include "simd/simd_detect.h"
#include <cstdint>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#include <cpuid.h>
#elif defined(__aarch64__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

SIMDType SIMDDetector::detectBestSIMD() {
#if defined(__x86_64__) || defined(__i386__)
    uint32_t eax, ebx, ecx, edx;

    // Check for AVX512
    __cpuid_count(7, 0, eax, ebx, ecx, edx);
    if (ebx & (1 << 16)) return SIMDType::AVX512;

    // Check for AVX2
    if (ebx & (1 << 5)) return SIMDType::AVX2;

    // Check for SSE4.2
    __cpuid(1, eax, ebx, ecx, edx);
    if (ecx & (1 << 20)) return SIMDType::SSE4_2;

#elif defined(__aarch64__)
    // Check for NEON on ARM
    int neon_supported = 0;
    size_t size = sizeof(neon_supported);
    if (sysctlbyname("hw.optional.neon", &neon_supported, &size, nullptr, 0) == 0 && neon_supported)
        return SIMDType::NEON;
#endif

    return SIMDType::SCALAR;
}

#include <iostream>

void SIMDDetector::printBestSIMD() {
    SIMDType type = detectBestSIMD();

    switch (type) {
        case SIMDType::UNKNOWN:
            std::cout << "UNKNOWN" << std::endl;
            break;
        case SIMDType::SSE4_2:
            std::cout << "SSE4.2" << std::endl;
            break;
        case SIMDType::AVX2:
            std::cout << "AVX2" << std::endl;
            break;
        case SIMDType::NEON:
            std::cout << "NEON" << std::endl;
            break;
        default:
            std::cout << "Unknown" << std::endl;
            break;
    }
}

