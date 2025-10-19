#include <iostream>
#include <cstring>
#include <immintrin.h>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <random>
#include <iomanip>

class SimdUtils {
public:
    // Optimized version for comparison
    static std::vector<std::pair<size_t, size_t>> findKeywordsInTextOptimized(
            const char* text, 
            size_t text_len, 
            const char* const keywords[], 
            size_t num_keywords,
            size_t max_keyword_len) {
            
        std::vector<std::pair<size_t, size_t>> results;
        
        if (text_len == 0) return results;
        
        // For each position in the text
        for (size_t i = 0; i <= text_len - 1; i++) {
            // Check if any keyword matches at this position
            for (size_t k = 0; k < num_keywords; k++) {
                size_t kw_len = std::strlen(keywords[k]);
                
                // Skip if keyword is empty or longer than remaining text
                if (kw_len == 0 || i + kw_len > text_len) continue;
                
                // Check the match
                bool match = true;
                for (size_t j = 0; j < kw_len; j++) {
                    if (text[i + j] != keywords[k][j]) {
                        match = false;
                        break;
                    }
                }
                
                if (match) {
                    results.push_back(std::make_pair(k, i));
                }
            }
        }
        
        return results;
    }

    // SIMD version with true AVX2 acceleration for small patterns
    static std::vector<std::pair<size_t, size_t>> findKeywordsInTextWithSIMD(
            const char* text, 
            size_t text_len, 
            const char* const keywords[], 
            size_t num_keywords) {
            
        std::vector<std::pair<size_t, size_t>> results;
        
        if (text_len == 0) return results;
        
        // For each keyword
        for (size_t k = 0; k < num_keywords; k++) {
            size_t kw_len = std::strlen(keywords[k]);
            
            // Skip empty keywords
            if (kw_len == 0) continue;
            
            // For short keywords that can fit in a SIMD register
            if (kw_len <= 16) {
                // Create pattern buffer for SIMD comparison
                alignas(32) char pattern[32] = {0};
                memcpy(pattern, keywords[k], kw_len);
                __m256i pattern_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(pattern));
                
                // Process text with sliding window
                for (size_t i = 0; i + kw_len <= text_len; i++) {
                    // For very short patterns (8 bytes or less), we can use a single 64-bit comparison
                    if (kw_len <= 8) {
                        uint64_t pattern_val = *reinterpret_cast<const uint64_t*>(pattern);
                        uint64_t text_val = 0;
                        memcpy(&text_val, text + i, kw_len);
                        // Mask out the bytes beyond pattern length
                        uint64_t mask = (1ULL << (kw_len * 8)) - 1;
                        if ((pattern_val & mask) == (text_val & mask)) {
                            results.push_back(std::make_pair(k, i));
                        }
                    }
                    // For patterns 9-16 bytes, use AVX2
                    else {
                        alignas(32) char chunk[32] = {0};
                        memcpy(chunk, text + i, kw_len);
                        __m256i text_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(chunk));
                        
                        // Compare bytes
                        __m256i cmp_result = _mm256_cmpeq_epi8(pattern_vec, text_vec);
                        int mask = _mm256_movemask_epi8(cmp_result);
                        
                        // Check if first kw_len bytes all match
                        if ((mask & ((1 << kw_len) - 1)) == ((1 << kw_len) - 1)) {
                            results.push_back(std::make_pair(k, i));
                        }
                    }
                }
            }
            // For longer keywords, use standard comparison
            else {
                for (size_t i = 0; i + kw_len <= text_len; i++) {
                    if (memcmp(text + i, keywords[k], kw_len) == 0) {
                        results.push_back(std::make_pair(k, i));
                    }
                }
            }
        }
        
        return results;
    }
};

// Function to generate random text
std::string generateRandomText(size_t length, std::mt19937& rng) {
    std::string text(length, ' ');
    std::uniform_int_distribution<> dist(97, 122); // a-z
    
    for (size_t i = 0; i < length; i++) {
        text[i] = static_cast<char>(dist(rng));
    }
    
    return text;
}

// Function to generate random keywords of specified length
std::vector<std::string> generateRandomKeywords(
    size_t count, size_t min_len, size_t max_len, std::mt19937& rng) {
    
    std::vector<std::string> keywords;
    std::uniform_int_distribution<> len_dist(min_len, max_len);
    std::uniform_int_distribution<> char_dist(97, 122); // a-z
    
    for (size_t i = 0; i < count; i++) {
        size_t len = len_dist(rng);
        std::string keyword(len, ' ');
        
        for (size_t j = 0; j < len; j++) {
            keyword[j] = static_cast<char>(char_dist(rng));
        }
        
        keywords.push_back(keyword);
    }
    
    return keywords;
}

// Function to run a benchmark
void runBenchmark(
    const std::string& text, 
    const std::vector<const char*>& keyword_ptrs,
    const std::vector<std::string>& keywords,
    size_t max_keyword_len,
    size_t num_runs) {
    
    size_t optimized_total_ms = 0;
    size_t simd_total_ms = 0;
    size_t optimized_match_count = 0;
    size_t simd_match_count = 0;
    
    for (size_t run = 0; run < num_runs; run++) {
        // Benchmark optimized version
        auto start = std::chrono::high_resolution_clock::now();
        auto optimized_results = SimdUtils::findKeywordsInTextOptimized(
            text.c_str(), text.length(), keyword_ptrs.data(), keyword_ptrs.size(), max_keyword_len);
        auto end = std::chrono::high_resolution_clock::now();
        optimized_total_ms += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        if (run == 0) {
            optimized_match_count = optimized_results.size();
        }
        
        // Benchmark SIMD version
        start = std::chrono::high_resolution_clock::now();
        auto simd_results = SimdUtils::findKeywordsInTextWithSIMD(
            text.c_str(), text.length(), keyword_ptrs.data(), keyword_ptrs.size());
        end = std::chrono::high_resolution_clock::now();
        simd_total_ms += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        if (run == 0) {
            simd_match_count = simd_results.size();
            
            // Verify results match
            if (optimized_match_count != simd_match_count) {
                std::cerr << "WARNING: Result count mismatch! Optimized: " 
                         << optimized_match_count << ", SIMD: " << simd_match_count << std::endl;
            } else {
                std::sort(optimized_results.begin(), optimized_results.end());
                std::sort(simd_results.begin(), simd_results.end());
                
                for (size_t i = 0; i < optimized_results.size(); i++) {
                    if (optimized_results[i] != simd_results[i]) {
                        std::cerr << "WARNING: Results differ at index " << i << std::endl;
                        break;
                    }
                }
            }
        }
    }
    
    double optimized_avg_ms = static_cast<double>(optimized_total_ms) / num_runs;
    double simd_avg_ms = static_cast<double>(simd_total_ms) / num_runs;
    double speedup = optimized_avg_ms / (simd_avg_ms > 0 ? simd_avg_ms : 1);
    
    std::cout << "Text size: " << text.length() / 1024 << " KB" << std::endl;
    std::cout << "Keyword count: " << keyword_ptrs.size() << std::endl;
    std::cout << "Max keyword length: " << max_keyword_len << " bytes" << std::endl;
    std::cout << "Matches found: " << optimized_match_count << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Optimized avg time: " << optimized_avg_ms << " ms" << std::endl;
    std::cout << "SIMD avg time: " << simd_avg_ms << " ms" << std::endl;
    std::cout << "Speedup: " << speedup << "x" << std::endl;
    
    double optimized_throughput = (text.length() / 1024.0) / (optimized_avg_ms / 1000.0);
    double simd_throughput = (text.length() / 1024.0) / (simd_avg_ms / 1000.0);
    
    std::cout << "Optimized throughput: " << optimized_throughput << " MB/s" << std::endl;
    std::cout << "SIMD throughput: " << simd_throughput << " MB/s" << std::endl;
    std::cout << std::endl;
}

int main() {
    std::mt19937 rng(42); // Fixed seed for reproducibility
    
    // Test parameters
    const size_t NUM_RUNS = 5;
    const std::vector<size_t> TEXT_SIZES = {100 * 1024, 1 * 1024 * 1024, 10 * 1024 * 1024};
    const std::vector<size_t> KEYWORD_COUNTS = {10, 50, 100};
    const std::vector<std::pair<size_t, size_t>> KEYWORD_LENGTHS = {
        {2, 4}, {4, 8}, {8, 12}
    };
    
    std::cout << "=== Benchmark for Small Pattern Keywords ===" << std::endl;
    std::cout << std::endl;
    
    for (size_t text_size : TEXT_SIZES) {
        std::string text = generateRandomText(text_size, rng);
        
        for (size_t keyword_count : KEYWORD_COUNTS) {
            for (const auto& [min_len, max_len] : KEYWORD_LENGTHS) {
                std::cout << "Testing text_size=" << text_size / 1024 << "KB, "
                         << "keyword_count=" << keyword_count << ", "
                         << "keyword_length=" << min_len << "-" << max_len << std::endl;
                
                auto keywords = generateRandomKeywords(keyword_count, min_len, max_len, rng);
                
                // Get max keyword length
                size_t max_keyword_len = 0;
                for (const auto& kw : keywords) {
                    max_keyword_len = std::max(max_keyword_len, kw.length());
                }
                
                // Insert some keywords into the text to ensure matches
                for (size_t i = 0; i < std::min(keyword_count, size_t(20)); i++) {
                    size_t insert_pos = (text_size / 20) * i;
                    if (insert_pos + keywords[i].length() <= text_size) {
                        memcpy(&text[insert_pos], keywords[i].c_str(), keywords[i].length());
                    }
                }
                
                // Convert keywords to C-style strings for the API
                std::vector<const char*> keyword_ptrs;
                for (const auto& kw : keywords) {
                    keyword_ptrs.push_back(kw.c_str());
                }
                
                runBenchmark(text, keyword_ptrs, keywords, max_keyword_len, NUM_RUNS);
                std::cout << "-------------------------------------" << std::endl;
            }
        }
    }
    
    return 0;
}