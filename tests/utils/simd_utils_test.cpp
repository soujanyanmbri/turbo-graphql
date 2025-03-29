#include "utils/SimdUtils.h"
#include "utils/SimdDetector.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <bitset>

class SimdUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        SimdUtils::initialize();
    }

    std::string createTestString(char target, size_t position) {
        std::string str(32, 'x');  // Fill with 'x'
        if (position < str.size()) {
            str[position] = target;
        }
        return str;
    }

    void verifyMask(const std::string& data, char target, const std::vector<size_t>& expectedPositions) {
        int mask = SimdUtils::charMask(data.c_str(), target);
        std::bitset<32> bits(mask);
        
        for (size_t pos : expectedPositions) {
            EXPECT_TRUE(bits.test(pos)) << "Expected target '" << target << "' at position " << pos;
        }
        
        // Check no other positions are set
        for (size_t i = 0; i < 32; i++) {
            if (std::find(expectedPositions.begin(), expectedPositions.end(), i) == expectedPositions.end()) {
                EXPECT_FALSE(bits.test(i)) << "Unexpected match at position " << i;
            }
        }
    }
};

TEST_F(SimdUtilsTest, SingleCharacterMatch) {
    for (size_t i = 0; i < 32; i++) {
        std::string data = createTestString('a', i);
        verifyMask(data, 'a', {i});
    }
}

TEST_F(SimdUtilsTest, MultipleCharacterMatches) {
    std::string data(32, 'x');
    data[5] = 'a';
    data[15] = 'a';
    data[25] = 'a';
    verifyMask(data, 'a', {5, 15, 25});
}

TEST_F(SimdUtilsTest, NoMatches) {
    std::string data(32, 'x');
    verifyMask(data, 'a', {});
}

TEST_F(SimdUtilsTest, AllMatches) {
    std::string data(32, 'a');
    std::vector<size_t> allPositions;
    for (size_t i = 0; i < 32; i++) {
        allPositions.push_back(i);
    }
    verifyMask(data, 'a', allPositions);
}

TEST_F(SimdUtilsTest, SpecialCharacters) {
    const std::vector<char> specialChars = {
        ' ', '\t', '\n', '\r',  // Whitespace
        '{', '}', '[', ']',     // Brackets
        ':', ',', '"', '\'',    // Punctuation
        '!', '@', '#', '$'      // Symbols
    };

    for (char c : specialChars) {
        for (size_t pos = 0; pos < 32; pos++) {
            std::string data = createTestString(c, pos);
            verifyMask(data, c, {pos});
        }
    }
}

TEST_F(SimdUtilsTest, BoundaryPositions) {
    verifyMask(createTestString('a', 0), 'a', {0});
    verifyMask(createTestString('a', 31), 'a', {31});
    
    std::string data(32, 'x');
    data[0] = 'a';
    data[31] = 'a';
    verifyMask(data, 'a', {0, 31});
}

// Test that the SIMD type detection works
TEST_F(SimdUtilsTest, SimdTypeDetection) {
    SIMDType type = SIMDDetector::detectBestSIMD();
    EXPECT_NE(type, SIMDType::UNKNOWN);
    
    // The actual type will depend on the CPU, but it should be one of our known types
    EXPECT_TRUE(type == SIMDType::SCALAR || 
                type == SIMDType::SSE4_2 || 
                type == SIMDType::AVX2 || 
                type == SIMDType::AVX512);
}
