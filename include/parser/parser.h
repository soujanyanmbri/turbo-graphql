#pragma once

#include <memory>
#include <vector>
#include <string>
#include <optional>
#include "ast/ast_nodes.h"
#include "ast/ast_arena.h"
#include "lexer/token/token.h"

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens, ASTArena& arena);
    
    // Main parsing entry point
    arena_ptr<Document> parse_document();
    
    // Get parsing errors
    const std::vector<std::string>& get_errors() const { return errors_; }
    bool has_errors() const { return !errors_.empty(); }

private:
    const std::vector<Token>& tokens_;
    size_t current_;
    ASTArena& arena_;
    std::vector<std::string> errors_;
    
    // Token navigation
    const Token& current_token() const;
    const Token& peek(size_t offset = 1) const;
    bool is_at_end() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool expect(TokenType type, const char* message);
    
    // Error handling
    void error(const std::string& message);
    void error_at_current(const std::string& message);
    void synchronize();  // Error recovery
    
    // Parsing methods
    arena_ptr<Document> parse_document_impl();
    Definition parse_definition();
    arena_ptr<OperationDefinition> parse_operation_definition();
    arena_ptr<FragmentDefinition> parse_fragment_definition();
    arena_ptr<SelectionSet> parse_selection_set();
    Selection parse_selection();
    arena_ptr<Field> parse_field();
    arena_ptr<FragmentSpread> parse_fragment_spread();
    arena_ptr<InlineFragment> parse_inline_fragment();
    
    // Arguments and directives
    std::vector<arena_ptr<Argument>> parse_arguments();
    arena_ptr<Argument> parse_argument();
    std::vector<arena_ptr<Directive>> parse_directives();
    arena_ptr<Directive> parse_directive();
    
    // Variables
    std::vector<arena_ptr<VariableDefinition>> parse_variable_definitions();
    arena_ptr<VariableDefinition> parse_variable_definition();
    arena_ptr<Variable> parse_variable();
    
    // Types
    arena_ptr<ASTNode> parse_type();
    arena_ptr<ASTNode> parse_named_type();
    arena_ptr<ASTNode> parse_list_type();
    
    // Values
    Value parse_value();
    Value parse_int_value();
    Value parse_float_value();
    Value parse_string_value();
    Value parse_boolean_value();
    Value parse_null_value();
    Value parse_enum_value();
    Value parse_list_value();
    Value parse_object_value();
    
    // Helpers
    OperationType parse_operation_type();
    std::string_view current_value() const;
    bool is_name_token() const;
};

