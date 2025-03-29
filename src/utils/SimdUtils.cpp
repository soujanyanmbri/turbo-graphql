#include "utils/SimdUtils.h"
#include <cstring>
#include <cstddef>
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
#elif defined(__ARM_NEON) || defined(__aarch64__)
    charMask = charMaskNEON;
#else
    charMask = charMaskScalar;
#endif
}

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
int SimdUtils::charMaskAVX512(const char* data, char target) {
#if defined(__AVX512BW__)
    __m512i chunk = _mm512_loadu_si512(data);
    return _mm512_cmpeq_epi8_mask(chunk, _mm512_set1_epi8(target));
#else
    return charMaskAVX2(data, target);
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


int SimdUtils::keywordMaskSSE (const char* data, size_t len, const char* keywords[], const size_t NUM_KEYWORDS) {
    if (len < 5) return 0; // Early exit if input is too short

    __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data));
    int mask = 0;

    for (int i = 0; i < NUM_KEYWORDS; i++) {
        size_t kw_len = std::strlen(keywords[i]);
        if (len < kw_len) continue;

        char keyword_buf[16] = {0};
        std::memcpy(keyword_buf, keywords[i], kw_len);
        __m128i kw_chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(keyword_buf));

        __m128i cmp = _mm_cmpeq_epi8(chunk, kw_chunk);
        int match_mask = _mm_movemask_epi8(cmp);

        int expected_mask = (1 << kw_len) - 1; // E.g., 0b11111 for "query" (5 letters)
        for (int j = 0; j <= 16 - kw_len; j++) {  // Scan within the chunk
            if ((match_mask >> j) == expected_mask) {
                mask |= (1 << i); // Mark keyword as found
                break;  // No need to check further
            }
        }
    }

    return mask;
}
int SimdUtils::keywordMaskAVX2 (const char* data, size_t len, const char* keywords[], const size_t NUM_KEYWORDS) {
    if (len < 5) return 0; // Early exit if input is too short
    __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data));
    int mask = 0;

    for (int i = 0; i < NUM_KEYWORDS; i++) {
        size_t kw_len = std::strlen(keywords[i]);
        if (len < kw_len) continue;  // Skip if input is shorter than the keyword

        // Load keyword as a 32-byte chunk (zero-padded if shorter)
        char keyword_buf[32] = {0}; // Zero padding ensures safe comparison
        std::memcpy(keyword_buf, keywords[i], kw_len);
        __m256i kw_chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keyword_buf));

        __m256i cmp = _mm256_cmpeq_epi8(chunk, kw_chunk);
        int match_mask = _mm256_movemask_epi8(cmp);

        int expected_mask = (1 << kw_len) - 1; // E.g., 0b11111 for "query" (5 letters)
        for (int j = 0; j <= 32 - kw_len; j++) {  // Scan within the chunk
            if ((match_mask >> j) == expected_mask) {
                mask |= (1 << i);
                break;
            }
        }
    }
    return mask;
}
#endif

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>

#include <arm_neon.h>

int SimdUtils::charMaskNEON(const char* data, char target) {
    // Create a vector filled with the target character
    uint8x16_t target_vec = vdupq_n_u8(target);
    
    // Load 16 bytes at a time from data
    uint8x16_t data_vec1 = vld1q_u8((const uint8_t*)data);
    uint8x16_t data_vec2 = vld1q_u8((const uint8_t*)(data + 16));
    
    // Compare data with target character
    uint8x16_t match_vec1 = vceqq_u8(data_vec1, target_vec);
    uint8x16_t match_vec2 = vceqq_u8(data_vec2, target_vec);
    
    // Convert match results to a bitmask
    uint16x8_t bit_vec1 = vmovl_u8(vget_low_u8(match_vec1));
    uint16x8_t bit_vec2 = vmovl_u8(vget_high_u8(match_vec1));
    uint16x8_t bit_vec3 = vmovl_u8(vget_low_u8(match_vec2));
    uint16x8_t bit_vec4 = vmovl_u8(vget_high_u8(match_vec2));
    
    // Shift and combine to form mask
    int mask = 0;
    mask |= vgetq_lane_u16(bit_vec1, 0) ? 1 : 0;
    mask |= vgetq_lane_u16(bit_vec1, 1) ? 2 : 0;
    mask |= vgetq_lane_u16(bit_vec1, 2) ? 4 : 0;
    mask |= vgetq_lane_u16(bit_vec1, 3) ? 8 : 0;
    mask |= vgetq_lane_u16(bit_vec1, 4) ? 16 : 0;
    mask |= vgetq_lane_u16(bit_vec1, 5) ? 32 : 0;
    mask |= vgetq_lane_u16(bit_vec1, 6) ? 64 : 0;
    mask |= vgetq_lane_u16(bit_vec1, 7) ? 128 : 0;
    
    mask |= vgetq_lane_u16(bit_vec2, 0) ? 256 : 0;
    mask |= vgetq_lane_u16(bit_vec2, 1) ? 512 : 0;
    mask |= vgetq_lane_u16(bit_vec2, 2) ? 1024 : 0;
    mask |= vgetq_lane_u16(bit_vec2, 3) ? 2048 : 0;
    mask |= vgetq_lane_u16(bit_vec2, 4) ? 4096 : 0;
    mask |= vgetq_lane_u16(bit_vec2, 5) ? 8192 : 0;
    mask |= vgetq_lane_u16(bit_vec2, 6) ? 16384 : 0;
    mask |= vgetq_lane_u16(bit_vec2, 7) ? 32768 : 0;
    
    mask |= vgetq_lane_u16(bit_vec3, 0) ? 65536 : 0;
    mask |= vgetq_lane_u16(bit_vec3, 1) ? 131072 : 0;
    mask |= vgetq_lane_u16(bit_vec3, 2) ? 262144 : 0;
    mask |= vgetq_lane_u16(bit_vec3, 3) ? 524288 : 0;
    mask |= vgetq_lane_u16(bit_vec3, 4) ? 1048576 : 0;
    mask |= vgetq_lane_u16(bit_vec3, 5) ? 2097152 : 0;
    mask |= vgetq_lane_u16(bit_vec3, 6) ? 4194304 : 0;
    mask |= vgetq_lane_u16(bit_vec3, 7) ? 8388608 : 0;
    
    mask |= vgetq_lane_u16(bit_vec4, 0) ? 16777216 : 0;
    mask |= vgetq_lane_u16(bit_vec4, 1) ? 33554432 : 0;
    mask |= vgetq_lane_u16(bit_vec4, 2) ? 67108864 : 0;
    mask |= vgetq_lane_u16(bit_vec4, 3) ? 134217728 : 0;
    mask |= vgetq_lane_u16(bit_vec4, 4) ? 268435456 : 0;
    mask |= vgetq_lane_u16(bit_vec4, 5) ? 536870912 : 0;
    mask |= vgetq_lane_u16(bit_vec4, 6) ? 1073741824 : 0;
    mask |= vgetq_lane_u16(bit_vec4, 7) ? (1U << 31) : 0;
    
    return mask;
}

int SimdUtils::keywordMaskNEON(const char* data, size_t len, const char keywords[][16], const size_t NUM_KEYWORDS){
    if (len < 5) return 0;
    uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data));
    int mask = 0;

    for (int i = 0; i < NUM_KEYWORDS; i++) {
        size_t kw_len = std::strlen(keywords[i]);
        if (len < kw_len) continue;

        char keyword_buf[16] = {0};
        std::memcpy(keyword_buf, keywords[i], kw_len);
        uint8x16_t kw_chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(keyword_buf));

        uint8x16_t cmp = vceqq_u8(chunk, kw_chunk);

        uint8_t cmp_array[16];
        vst1q_u8(cmp_array, cmp);

        uint16_t match_mask = 0;
        for (int j = 0; j < 16; j++) {
            match_mask |= (cmp_array[j] & 0x80) ? (1 << j) : 0;
        }

        int expected_mask = (1 << kw_len) - 1; // Example: 0b11111 for "query" (5 letters)
        for (int j = 0; j <= 16 - kw_len; j++) {  
            if ((match_mask >> j) == expected_mask) {
                mask |= (1 << i); // Mark keyword as found
                break;
            }
        }
    }

    return mask;
}

#endif

int SimdUtils::charMaskScalar(const char* data, char target) {
    int mask = 0;
    for (int i = 0; i < 32; i++) {
        if (data[i] == target) mask |= (1 << i);
    }
    return mask;
}
