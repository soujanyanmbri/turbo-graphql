#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include "simd/simd_detect.h"
#include "lexer/lexer.h"
#include "lexer/token/token_arena.h"
#include "lexer/token/token.h"
#include "parser/parser.h"

// Helper function to convert TokenType to string for display
const char* tokenTypeToString(TokenType type) {
    switch (type) {
        case KEYWORD_QUERY: return "KEYWORD_QUERY";
        case KEYWORD_MUTATION: return "KEYWORD_MUTATION";
        case KEYWORD_SUBSCRIPTION: return "KEYWORD_SUBSCRIPTION";
        case KEYWORD_FRAGMENT: return "KEYWORD_FRAGMENT";
        case KEYWORD_ON: return "KEYWORD_ON";
        case KEYWORD_TRUE: return "KEYWORD_TRUE";
        case KEYWORD_FALSE: return "KEYWORD_FALSE";
        case KEYWORD_NULL: return "KEYWORD_NULL";
        case KEYWORD_TYPE: return "KEYWORD_TYPE";
        case KEYWORD_INPUT: return "KEYWORD_INPUT";
        case KEYWORD_ENUM: return "KEYWORD_ENUM";
        case KEYWORD_INTERFACE: return "KEYWORD_INTERFACE";
        case KEYWORD_UNION: return "KEYWORD_UNION";
        case KEYWORD_DIRECTIVE: return "KEYWORD_DIRECTIVE";
        case KEYWORD_SCALAR: return "KEYWORD_SCALAR";
        case KEYWORD_EXTEND: return "KEYWORD_EXTEND";
        case KEYWORD_IMPLEMENTS: return "KEYWORD_IMPLEMENTS";
        case KEYWORD_TYPENAME: return "KEYWORD_TYPENAME";
        case KEYWORD_SCHEMA: return "KEYWORD_SCHEMA";
        case KEYWORD_TYPE_META: return "KEYWORD_TYPE_META";
        case KEYWORD_GET: return "KEYWORD_GET";
        case KEYWORD_CREATE: return "KEYWORD_CREATE";
        case KEYWORD_UPDATE: return "KEYWORD_UPDATE";
        case KEYWORD_DELETE: return "KEYWORD_DELETE";
        case KEYWORD_INT: return "KEYWORD_INT";
        case KEYWORD_FLOAT: return "KEYWORD_FLOAT";
        case KEYWORD_STRING: return "KEYWORD_STRING";
        case KEYWORD_BOOLEAN: return "KEYWORD_BOOLEAN";
        case KEYWORD_ID: return "KEYWORD_ID";
        case IDENTIFIER: return "IDENTIFIER";
        case VARIABLE: return "VARIABLE";
        case DIRECTIVE: return "DIRECTIVE";
        case NUMBER: return "NUMBER";
        case STRING: return "STRING";
        case SYMBOL: return "SYMBOL";
        case LEFT_BRACE: return "LEFT_BRACE";
        case RIGHT_BRACE: return "RIGHT_BRACE";
        case LEFT_PAREN: return "LEFT_PAREN";
        case RIGHT_PAREN: return "RIGHT_PAREN";
        case LEFT_BRACKET: return "LEFT_BRACKET";
        case RIGHT_BRACKET: return "RIGHT_BRACKET";
        case COLON: return "COLON";
        case COMMA: return "COMMA";
        case ELLIPSIS: return "ELLIPSIS";
        case EXCLAMATION: return "EXCLAMATION";
        case BOOLEAN: return "BOOLEAN";
        case NULL_VALUE: return "NULL_VALUE";
        case UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

// Forward declarations for recursive printing
void printSelectionSet(const SelectionSet& sel_set, int indent);
void printValue(const Value& value, int indent);

void printField(const Field& field, int indent) {
    std::string ind(indent * 2, ' ');
    std::cout << ind << "Field: ";
    
    if (!field.alias.empty()) {
        std::cout << field.alias << ": ";
    }
    std::cout << field.name;
    
    if (!field.arguments.empty()) {
        std::cout << "(";
        for (size_t i = 0; i < field.arguments.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << field.arguments[i]->name << ": ";
            // Print a simplified value representation
            if (std::holds_alternative<IntValue>(field.arguments[i]->value)) {
                std::cout << std::get<IntValue>(field.arguments[i]->value).value;
            } else if (std::holds_alternative<StringValue>(field.arguments[i]->value)) {
                std::cout << "\"" << std::get<StringValue>(field.arguments[i]->value).value << "\"";
            } else if (auto* var = std::get_if<std::unique_ptr<Variable>>(&field.arguments[i]->value)) {
                std::cout << "$" << (*var)->name;
            } else {
                std::cout << "...";
            }
        }
        std::cout << ")";
    }
    
    if (!field.directives.empty()) {
        for (const auto& dir : field.directives) {
            std::cout << " @" << dir->name;
            if (!dir->arguments.empty()) {
                std::cout << "(...)";
            }
        }
    }
    
    std::cout << "\n";
    
    if (field.selection_set) {
        printSelectionSet(*field.selection_set, indent + 1);
    }
}

void printSelectionSet(const SelectionSet& sel_set, int indent) {
    for (const auto& selection : sel_set.selections) {
        if (auto* field_ptr = std::get_if<std::unique_ptr<Field>>(&selection)) {
            printField(**field_ptr, indent);
        } else if (auto* spread_ptr = std::get_if<std::unique_ptr<FragmentSpread>>(&selection)) {
            std::string ind(indent * 2, ' ');
            std::cout << ind << "..." << (*spread_ptr)->name << "\n";
        } else if (auto* inline_ptr = std::get_if<std::unique_ptr<InlineFragment>>(&selection)) {
            std::string ind(indent * 2, ' ');
            std::cout << ind << "... on " << (*inline_ptr)->type_condition << "\n";
            if ((*inline_ptr)->selection_set) {
                printSelectionSet(*(*inline_ptr)->selection_set, indent + 1);
            }
        }
    }
}

void printAST(const Document& doc, int indent = 0) {
    std::string ind(indent * 2, ' ');
    std::cout << ind << "Document with " << doc.definitions.size() << " definition(s)\n\n";
    
    for (size_t i = 0; i < doc.definitions.size(); i++) {
        const auto& def = doc.definitions[i];
        
        if (std::holds_alternative<std::unique_ptr<OperationDefinition>>(def)) {
            const auto& op = std::get<std::unique_ptr<OperationDefinition>>(def);
            std::cout << ind << "[" << i << "] ";
            
            switch (op->operation_type) {
                case OperationType::QUERY: std::cout << "QUERY"; break;
                case OperationType::MUTATION: std::cout << "MUTATION"; break;
                case OperationType::SUBSCRIPTION: std::cout << "SUBSCRIPTION"; break;
            }
            
            if (!op->name.empty()) {
                std::cout << " " << op->name;
            }
            
            // Print variables
            if (!op->variable_definitions.empty()) {
                std::cout << "(";
                for (size_t j = 0; j < op->variable_definitions.size(); j++) {
                    if (j > 0) std::cout << ", ";
                    std::cout << "$" << op->variable_definitions[j]->variable->name << ": ";
                    // Print type (simplified)
                    auto& type_node = op->variable_definitions[j]->type;
                    if (std::holds_alternative<NamedType>(type_node->data)) {
                        std::cout << std::get<NamedType>(type_node->data).name;
                    } else if (std::holds_alternative<NonNullType>(type_node->data)) {
                        auto& nnt = std::get<NonNullType>(type_node->data);
                        if (std::holds_alternative<NamedType>(nnt.type->data)) {
                            std::cout << std::get<NamedType>(nnt.type->data).name << "!";
                        }
                    }
                }
                std::cout << ")";
            }
            
            std::cout << " {\n";
            
            if (op->selection_set) {
                printSelectionSet(*op->selection_set, indent + 1);
            }
            
            std::cout << ind << "}\n\n";
            
        } else if (std::holds_alternative<std::unique_ptr<FragmentDefinition>>(def)) {
            const auto& frag = std::get<std::unique_ptr<FragmentDefinition>>(def);
            std::cout << ind << "[" << i << "] FRAGMENT " << frag->name 
                     << " on " << frag->type_condition << " {\n";
                     
            if (frag->selection_set) {
                printSelectionSet(*frag->selection_set, indent + 1);
            }
            
            std::cout << ind << "}\n\n";
        }
    }
}

int main(int argc, char* argv[]) {
    // Print header
    std::cout << "\n====================================================================\n";
    std::cout << "         TURBO-GRAPHQL: High-Performance GraphQL Parser            \n";
    std::cout << "====================================================================\n\n";
    
    // Detect and display SIMD capabilities
    std::cout << "SIMD Detection: ";
    SIMDDetector::printBestSIMD();
    std::cout << std::endl;
    
    // Sample GraphQL query
    const char* sample_query = R"(
query GetUser($userId: ID!) {
  user(id: $userId) {
    name
    email
    posts @include(if: true) {
      title
      content
    }
  }
}
)";
    
    // Check if a file was provided as argument
    std::string query_content;
    const char* query_to_parse = nullptr;
    size_t query_length = 0;
    
    if (argc > 1) {
        // Read from file
        std::ifstream file(argv[1]);
        if (file.good()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            query_content = buffer.str();
            query_to_parse = query_content.c_str();
            query_length = query_content.length();
            std::cout << "Parsing file: " << argv[1] << "\n";
        } else {
            std::cerr << "Error: Could not open file '" << argv[1] << "'\n";
            std::cerr << "Using sample query instead.\n\n";
            query_to_parse = sample_query;
            query_length = strlen(sample_query);
        }
    } else {
        query_to_parse = sample_query;
        query_length = strlen(sample_query);
        std::cout << "Using sample query (provide a .graphql file as argument to parse it)\n";
    }
    
    // Display the query
    std::cout << "\n" << std::string(68, '-') << "\n";
    std::cout << "Query to parse:\n";
    std::cout << std::string(68, '-') << "\n";
    std::cout << query_to_parse << "\n";
    std::cout << std::string(68, '-') << "\n";
    
    // Tokenize the query
    std::cout << "\n[1/2] Tokenizing with SIMD-accelerated lexer...\n";
    
    TokenArena arena;
    Tokenizer tokenizer;
    
    auto start_lex = std::chrono::high_resolution_clock::now();
    auto& tokens = tokenizer.tokenize(query_to_parse, query_length, arena);
    auto end_lex = std::chrono::high_resolution_clock::now();
    
    auto lex_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_lex - start_lex);
    
    std::cout << "   Tokenization completed in " << lex_duration.count() << " microseconds\n";
    std::cout << "   Generated " << tokens.size() << " tokens\n";
    
    // Parse the tokens into AST
    std::cout << "\n[2/2] Parsing tokens into AST...\n";
    
    // Convert pmr::vector to std::vector for parser
    std::vector<Token> token_vec(tokens.begin(), tokens.end());
    Parser parser(token_vec);
    
    auto start_parse = std::chrono::high_resolution_clock::now();
    auto ast = parser.parse_document();
    auto end_parse = std::chrono::high_resolution_clock::now();
    
    auto parse_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_parse - start_parse);
    
    std::cout << "   Parsing completed in " << parse_duration.count() << " microseconds\n";
    
    // Check for parsing errors
    if (parser.has_errors()) {
        std::cout << "\n" << std::string(68, '=') << "\n";
        std::cout << "PARSING ERRORS:\n";
        std::cout << std::string(68, '=') << "\n";
        for (const auto& error : parser.get_errors()) {
            std::cout << "  ERROR: " << error << "\n";
        }
        std::cout << std::string(68, '=') << "\n";
    } else {
        std::cout << "   No parsing errors!\n";
    }
    
    // Print AST summary
    if (ast) {
        std::cout << "\n" << std::string(68, '=') << "\n";
        std::cout << "AST STRUCTURE:\n";
        std::cout << std::string(68, '=') << "\n";
        printAST(*ast);
        std::cout << std::string(68, '=') << "\n";
    }
    
    // Print statistics
    auto total_duration = lex_duration + parse_duration;
    std::cout << "\n" << std::string(68, '=') << "\n";
    std::cout << "PERFORMANCE SUMMARY:\n";
    std::cout << std::string(68, '=') << "\n";
    std::cout << "  Input size:      " << query_length << " bytes\n";
    std::cout << "  Tokens:          " << tokens.size() << "\n";
    std::cout << "  Lexing time:     " << lex_duration.count() << " µs\n";
    std::cout << "  Parsing time:    " << parse_duration.count() << " µs\n";
    std::cout << "  Total time:      " << total_duration.count() << " µs\n";
    
    if (total_duration.count() > 0) {
        double throughput = (query_length / (total_duration.count() / 1000000.0)) / (1024.0 * 1024.0);
        std::cout << "  Throughput:      " << std::fixed << std::setprecision(2) 
                  << throughput << " MB/s\n";
    }
    std::cout << std::string(68, '=') << "\n\n";
    
    std::cout << "Example usage:\n";
    std::cout << "  ./build/graphql_parser                  # Parse sample query\n";
    std::cout << "  ./build/graphql_parser query.graphql   # Parse specific file\n\n";
    
    return 0;
}
