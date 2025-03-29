#include "utils/SimdUtils.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#endif

SIMDType SimdUtils::activeSIMD = SIMDType::SCALAR;
SIMDCharMaskFunc SimdUtils::charMask = SimdUtils::charMaskScalar;

void SimdUtils::initialize() {
    activeSIMD = SIMDDetector::detectBestSIMD();

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    switch (activeSIMD) {
        case SIMDType::AVX512: charMask = charMaskAVX512; break;
        case SIMDType::AVX2:   charMask = charMaskAVX2; break;
        case SIMDType::SSE4_2: charMask = charMaskSSE; break;
        default:               charMask = charMaskScalar; break;
    }
#else
    charMask = charMaskScalar;
#endif
}

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
int SimdUtils::charMaskAVX512(const char* data, char target) {
#if defined(__AVX512BW__)  // Ensure the compiler actually supports AVX-512BW
    __m512i chunk = _mm512_loadu_si512(data);
    return _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8(target));
#else
    return charMaskAVX2(data, target);  // Fallback to AVX2
#endif
}

int SimdUtils::charMaskAVX2(const char* data, char target) {
    __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data));
    return _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, _mm256_set1_epi8(target)));
}

int SimdUtils::charMaskSSE(const char* data, char target) {
    __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data));
    return _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, _mm_set1_epi8(target)));
}
#endif

int SimdUtils::charMaskScalar(const char* data, char target) {
    int mask = 0;
    for (int i = 0; i < 32; i++) {
        if (data[i] == target) mask |= (1 << i);
    }
    return mask;
}
