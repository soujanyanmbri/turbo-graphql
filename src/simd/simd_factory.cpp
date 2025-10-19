#include "simd/simd_factory.h"
#include "simd/impl/avx2_impl.h"
#include "simd/impl/avx512_impl.h"
#include "simd/impl/neon_impl.h"
#include "simd/impl/sse_impl.h"
#include "simd/impl/scalar_impl.h"
#include "simd/simd_detect.h"

SIMDInterface* createBestSIMDImplementation() {
    switch (SIMDDetector::detectBestSIMD()) {
        case SIMDType::AVX512: 
            // AVX512 implementation not yet complete, fall back to AVX2
            return new AVX2TextProcessor();
        case SIMDType::AVX2: 
            return new AVX2TextProcessor();
        case SIMDType::SSE4_2: 
            return new SSETextProcessor();
        case SIMDType::SSE2: 
            return new SSETextProcessor();
        case SIMDType::NEON: 
            // NEON implementation not yet complete, fall back to Scalar
            return new ScalarTextProcessor();
        case SIMDType::SCALAR:
        default: 
            return new ScalarTextProcessor();
    }
}
