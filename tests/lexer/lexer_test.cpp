// #include "lexer/Lexer.h"
// #include <gtest/gtest.h>
// #include <vector>
// #include <string>

// class TokenizerTest : public ::testing::Test {
// protected:
//     void SetUp() override {}

//     void verifyTokens(const std::string& input, const std::vector<Token>& expectedTokens) {
//         Tokenizer tokenizer(input);
//         std::vector<Token> tokens = tokenizer.tokenize();
        
//         ASSERT_EQ(tokens.size(), expectedTokens.size()) 
//             << "Expected " << expectedTokens.size() << " tokens, but got " << tokens.size();
        
//         for (size_t i = 0; i < tokens.size(); i++) {
//             EXPECT_EQ(tokens[i].type, expectedTokens[i].type)
//                 << "Token mismatch at position " << i << ": " << tokens[i].value << " != " << expectedTokens[i].value;
//             EXPECT_EQ(tokens[i].value, expectedTokens[i].value)
//                 << "Value mismatch at position " << i << ": " << tokens[i].value << " != " << expectedTokens[i].value;
//         }
//     }
// };

// // Test basic structural tokens
// TEST_F(TokenizerTest, StructuralTokens) {
//     std::string input = "{ } ( ) [ ] : ,";
//     std::vector<Token> expected = {
//         {TokenType::LEFT_BRACE, "{"},
//         {TokenType::RIGHT_BRACE, "}"},
//         {TokenType::LEFT_PAREN, "("},
//         {TokenType::RIGHT_PAREN, ")"},
//         {TokenType::LEFT_BRACKET, "["},
//         {TokenType::RIGHT_BRACKET, "]"},
//         {TokenType::COLON, ":"},
//         {TokenType::COMMA, ","}
//     };
//     verifyTokens(input, expected);
// }

// // Test string literals
// // TEST_F(TokenizerTest, StringLiterals) {
// //     std::string input = R"("simple" "with spaces" "with \"quotes\"")";
// //     std::vector<Token> expected = {
// //         {TokenType::STRING, "simple"},
// //         {TokenType::STRING, "with spaces"},
// //         {TokenType::STRING, "with \"quotes\""}
// //     };
// //     verifyTokens(input, expected);
// // }

// // Test number literals
// // TEST_F(TokenizerTest, NumberLiterals) {
// //     std::string input = "42 3.14 -17 -0.001";
// //     std::vector<Token> expected = {
// //         {TokenType::NUMBER, "42"},
// //         {TokenType::NUMBER, "3.14"},
// //         {TokenType::NUMBER, "-17"},
// //         {TokenType::NUMBER, "-0.001"}
// //     };
// //     verifyTokens(input, expected);
// // }

// // Test identifiers and keywords
// TEST_F(TokenizerTest, IdentifiersAndKeywords) {
//     std::string input = "query mutation subscription type enum interface union input scalar";
//     std::vector<Token> expected = {
//         {TokenType::IDENTIFIER, "query"},
//         {TokenType::IDENTIFIER, "mutation"},
//         {TokenType::IDENTIFIER, "subscription"},
//         {TokenType::IDENTIFIER, "type"},
//         {TokenType::IDENTIFIER, "enum"},
//         {TokenType::IDENTIFIER, "interface"},
//         {TokenType::IDENTIFIER, "union"},
//         {TokenType::IDENTIFIER, "input"},
//         {TokenType::IDENTIFIER, "scalar"}
//     };
//     verifyTokens(input, expected);
// }

// // Test boolean and null literals
// // TEST_F(TokenizerTest, BooleanAndNull) {
// //     std::string input = "true false null";
// //     std::vector<Token> expected = {
// //         {TokenType::BOOLEAN, "true"},
// //         {TokenType::BOOLEAN, "false"},
// //         {TokenType::NULL_VALUE, "null"}
// //     };
// //     verifyTokens(input, expected);
// // }

// // Test complex GraphQL query
// TEST_F(TokenizerTest, ComplexQuery) {
//     std::string input = R"(
//         query GetUser {
//             user(id: "123") {
//                 name
//                 age
//                 posts @include(if: true) {
//                     title
//                     content
//                 }
//             }
//         }
//     )";
    
//     std::vector<Token> expected = {
//         {TokenType::IDENTIFIER, "query"},
//         {TokenType::IDENTIFIER, "GetUser"},
//         {TokenType::LEFT_BRACE, "{"},
//         {TokenType::IDENTIFIER, "user"},
//         {TokenType::LEFT_PAREN, "("},
//         {TokenType::IDENTIFIER, "id"},
//         {TokenType::COLON, ":"},
//         {TokenType::STRING, "123"},
//         {TokenType::RIGHT_PAREN, ")"},
//         {TokenType::LEFT_BRACE, "{"},
//         {TokenType::IDENTIFIER, "name"},
//         {TokenType::IDENTIFIER, "age"},
//         {TokenType::IDENTIFIER, "posts"},
//         {TokenType::DIRECTIVE, "@include"},
//         {TokenType::LEFT_PAREN, "("},
//         {TokenType::IDENTIFIER, "if"},
//         {TokenType::COLON, ":"},
//         {TokenType::BOOLEAN, "true"},
//         {TokenType::RIGHT_PAREN, ")"},
//         {TokenType::LEFT_BRACE, "{"},
//         {TokenType::IDENTIFIER, "title"},
//         {TokenType::IDENTIFIER, "content"},
//         {TokenType::RIGHT_BRACE, "}"},
//         {TokenType::RIGHT_BRACE, "}"},
//         {TokenType::RIGHT_BRACE, "}"}
//     };
//     verifyTokens(input, expected);
// }

// // Test whitespace handling
// // TEST_F(TokenizerTest, WhitespaceHandling) {
// //     std::string input = "\n\t  {  \n  }  \r\n";
// //     std::vector<Token> expected = {
// //         {TokenType::LEFT_BRACE, "{"},
// //         {TokenType::RIGHT_BRACE, "}"}
// //     };
// //     verifyTokens(input, expected);
// // }

// // // Test error cases
// // TEST_F(TokenizerTest, ErrorCases) {
// //     // Unterminated string
// //     EXPECT_THROW({
// //         Tokenizer tokenizer("\"unterminated");
// //         tokenizer.tokenize();
// //     }, std::runtime_error);

// //     // Invalid number format
// //     EXPECT_THROW({
// //         Tokenizer tokenizer("42.42.42");
// //         tokenizer.tokenize();
// //     }, std::runtime_error);

// //     // Invalid character
// //     EXPECT_THROW({
// //         Tokenizer tokenizer("$invalid");
// //         tokenizer.tokenize();
// //     }, std::runtime_error);
// // }

// // // Test SIMD vs non-SIMD mode
// // TEST_F(TokenizerTest, SimdVsNonSimd) {
// //     std::string input = R"(
// //         query {
// //             field1
// //             field2
// //             field3
// //         }
// //     )";

// //     // Test with SIMD enabled
// //     Tokenizer::UseSIMD = true;
// //     std::vector<Token> simdTokens = Tokenizer(input).tokenize();

// //     // Test with SIMD disabled
// //     Tokenizer::UseSIMD = false;
// //     std::vector<Token> nonSimdTokens = Tokenizer(input).tokenize();

// //     // Results should be identical regardless of SIMD usage
// //     ASSERT_EQ(simdTokens.size(), nonSimdTokens.size());
// //     for (size_t i = 0; i < simdTokens.size(); i++) {
// //         EXPECT_EQ(simdTokens[i].type, nonSimdTokens[i].type);
// //         EXPECT_EQ(simdTokens[i].value, nonSimdTokens[i].value);
// //     }
// // }

// // // Test long input handling
// // TEST_F(TokenizerTest, LongInput) {
// //     // Create a string longer than 32 bytes to test SIMD chunking
// //     std::string input = std::string(100, ' ') + "query" + std::string(100, ' ') + "{" + 
// //                        std::string(100, ' ') + "field" + std::string(100, ' ') + "}";
    
// //     std::vector<Token> expected = {
// //         {TokenType::IDENTIFIER, "query"},
// //         {TokenType::LEFT_BRACE, "{"},
// //         {TokenType::IDENTIFIER, "field"},
// //         {TokenType::RIGHT_BRACE, "}"}
// //     };
// //     verifyTokens(input, expected);
// // }