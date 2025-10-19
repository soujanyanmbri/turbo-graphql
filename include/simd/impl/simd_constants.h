#pragma once

#include <cstdint>

// Include SIMD headers based on architecture
#ifdef _MSC_VER
    #include <intrin.h>
#else
    #include <immintrin.h>
    #include <emmintrin.h>
    #ifdef __ARM_NEON
        #include <arm_neon.h>
    #endif
#endif

#include "simd/simd_detect.h" // Include your detector header

namespace simd_constants {

constexpr char CHAR_SPACE = ' ';
constexpr char CHAR_TAB = '\t';
constexpr char CHAR_NEWLINE = '\n';
constexpr char CHAR_CARRIAGE_RETURN = '\r';
constexpr char CHAR_UNDERSCORE = '_';
constexpr char CHAR_STAR = '*';
constexpr char CHAR_0 = '0';
constexpr char CHAR_9 = '9';
constexpr char CHAR_A = 'A';
constexpr char CHAR_Z = 'Z';
constexpr char CHAR_a = 'a';
constexpr char CHAR_z = 'z';
constexpr char CHAR_DOT = '.';
constexpr char CHAR_ESCAPE = '\\';

#ifdef __AVX512F__
    alignas(64) static const __m512i avx512_whitespace_bytes = _mm512_set1_epi32(
        (CHAR_CARRIAGE_RETURN << 24) | (CHAR_NEWLINE << 16) | (CHAR_TAB << 8) | CHAR_SPACE);
    alignas(64) static const __m512i avx512_underscore = _mm512_set1_epi8(CHAR_UNDERSCORE);
    alignas(64) static const __m512i avx512_gt_zero = _mm512_set1_epi8(CHAR_0 - 1);
    alignas(64) static const __m512i avx512_lt_nine = _mm512_set1_epi8(CHAR_9 + 1);
    alignas(64) static const __m512i avx512_gt_a = _mm512_set1_epi8(CHAR_a - 1);
    alignas(64) static const __m512i avx512_lt_z = _mm512_set1_epi8(CHAR_z + 1);
    alignas(64) static const __m512i avx512_gt_A = _mm512_set1_epi8(CHAR_A - 1);
    alignas(64) static const __m512i avx512_lt_Z = _mm512_set1_epi8(CHAR_Z + 1);
    alignas(64) static const __m512i avx512_space = _mm512_set1_epi8(CHAR_SPACE);
    alignas(64) static const __m512i avx512_tab = _mm512_set1_epi8(CHAR_TAB);
    alignas(64) static const __m512i avx512_nl = _mm512_set1_epi8(CHAR_NEWLINE);
    alignas(64) static const __m512i avx512_cr = _mm512_set1_epi8(CHAR_CARRIAGE_RETURN);
    alignas(64) static const __m512i avx512_star_v = _mm512_set1_epi8(CHAR_STAR);
    alignas(64) static const __m512i avx512_dot = _mm512_set1_epi8(CHAR_DOT);
    alignas(64) static const __m512i avx512_escape = _mm512_set1_epi8(CHAR_ESCAPE);
#endif

#ifdef __AVX2__
    alignas(32) static const __m256i avx2_whitespace_bytes = _mm256_setr_epi8(
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN
    );
    alignas(32) static const __m256i avx2_underscore = _mm256_set1_epi8(CHAR_UNDERSCORE);
    alignas(32) static const __m256i avx2_gt_zero = _mm256_set1_epi8(CHAR_0 - 1);
    alignas(32) static const __m256i avx2_lt_nine = _mm256_set1_epi8(CHAR_9 + 1);
    alignas(32) static const __m256i avx2_gt_a = _mm256_set1_epi8(CHAR_a - 1);
    alignas(32) static const __m256i avx2_lt_z = _mm256_set1_epi8(CHAR_z + 1);
    alignas(32) static const __m256i avx2_gt_A = _mm256_set1_epi8(CHAR_A - 1);
    alignas(32) static const __m256i avx2_lt_Z = _mm256_set1_epi8(CHAR_Z + 1);
    alignas(32) static const __m256i avx2_space = _mm256_set1_epi8(CHAR_SPACE);
    alignas(32) static const __m256i avx2_tab = _mm256_set1_epi8(CHAR_TAB);
    alignas(32) static const __m256i avx2_nl = _mm256_set1_epi8(CHAR_NEWLINE);
    alignas(32) static const __m256i avx2_cr = _mm256_set1_epi8(CHAR_CARRIAGE_RETURN);
    alignas(32) static const __m256i avx2_star_v = _mm256_set1_epi8(CHAR_STAR);
    alignas(32) static const __m256i avx2_dot = _mm256_set1_epi8(CHAR_DOT);
    alignas(32) static const __m256i avx2_escape = _mm256_set1_epi8(CHAR_ESCAPE);
#endif

#ifdef __SSE4_2__
    alignas(16) static const __m128i sse_whitespace_bytes = _mm_setr_epi8(
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN
    );
    alignas(16) static const __m128i sse_underscore = _mm_set1_epi8(CHAR_UNDERSCORE);
    alignas(16) static const __m128i sse_gt_zero = _mm_set1_epi8(CHAR_0 - 1);
    alignas(16) static const __m128i sse_lt_nine = _mm_set1_epi8(CHAR_9 + 1);
    alignas(16) static const __m128i sse_gt_a = _mm_set1_epi8(CHAR_a - 1);
    alignas(16) static const __m128i sse_lt_z = _mm_set1_epi8(CHAR_z + 1);
    alignas(16) static const __m128i sse_gt_A = _mm_set1_epi8(CHAR_A - 1);
    alignas(16) static const __m128i sse_lt_Z = _mm_set1_epi8(CHAR_Z + 1);
    alignas(16) static const __m128i sse_space = _mm_set1_epi8(CHAR_SPACE);
    alignas(16) static const __m128i sse_tab = _mm_set1_epi8(CHAR_TAB);
    alignas(16) static const __m128i sse_nl = _mm_set1_epi8(CHAR_NEWLINE);
    alignas(16) static const __m128i sse_cr = _mm_set1_epi8(CHAR_CARRIAGE_RETURN);
    alignas(16) static const __m128i sse_star_v = _mm_set1_epi8(CHAR_STAR);
    alignas(16) static const __m128i sse_dot = _mm_set1_epi8(CHAR_DOT);
    alignas(16) static const __m128i sse_escape = _mm_set1_epi8(CHAR_ESCAPE);
#endif

#ifdef __ARM_NEON
    alignas(16) static const uint8x16_t neon_whitespace_bytes = {
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN,
        CHAR_SPACE, CHAR_TAB, CHAR_NEWLINE, CHAR_CARRIAGE_RETURN
    };
    alignas(16) static const uint8x16_t neon_underscore = vdupq_n_u8(CHAR_UNDERSCORE);
    alignas(16) static const uint8x16_t neon_gt_zero = vdupq_n_u8(CHAR_0 - 1);
    alignas(16) static const uint8x16_t neon_lt_nine = vdupq_n_u8(CHAR_9 + 1);
    alignas(16) static const uint8x16_t neon_gt_a = vdupq_n_u8(CHAR_a - 1);
    alignas(16) static const uint8x16_t neon_lt_z = vdupq_n_u8(CHAR_z + 1);
    alignas(16) static const uint8x16_t neon_gt_A = vdupq_n_u8(CHAR_A - 1);
    alignas(16) static const uint8x16_t neon_lt_Z = vdupq_n_u8(CHAR_Z + 1);
    alignas(16) static const uint8x16_t neon_space = vdupq_n_u8(CHAR_SPACE);
    alignas(16) static const uint8x16_t neon_tab = vdupq_n_u8(CHAR_TAB);
    alignas(16) static const uint8x16_t neon_nl = vdupq_n_u8(CHAR_NEWLINE);
    alignas(16) static const uint8x16_t neon_cr = vdupq_n_u8(CHAR_CARRIAGE_RETURN);
    alignas(16) static const uint8x16_t neon_star_v = vdupq_n_u8(CHAR_STAR);
    alignas(16) static const uint8x16_t neon_dot = vdupq_n_u8(CHAR_DOT);
    alignas(16) static const uint8x16_t neon_escape = vdupq_n_u8(CHAR_ESCAPE);
#endif
}