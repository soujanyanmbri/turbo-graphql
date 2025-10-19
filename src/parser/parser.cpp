#include "parser/parser.h"
#include <sstream>

Parser::Parser(const std::vector<Token>& tokens, ASTArena& arena)
    : tokens_(tokens), current_(0), arena_(arena) {}

// Token navigation
const Token& Parser::current_token() const {
    if (current_ >= tokens_.size()) {
        static Token eof_token{TokenType::UNKNOWN, "", 0};
        return eof_token;
    }
    return tokens_[current_];
}

const Token& Parser::peek(size_t offset) const {
    size_t pos = current_ + offset;
    if (pos >= tokens_.size()) {
        static Token eof_token{TokenType::UNKNOWN, "", 0};
        return eof_token;
    }
    return tokens_[pos];
}

bool Parser::is_at_end() const {
    return current_ >= tokens_.size();
}

const Token& Parser::advance() {
    if (!is_at_end()) current_++;
    return tokens_[current_ - 1];
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) return false;
    return current_token().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::expect(TokenType type, const char* message) {
    if (check(type)) {
        advance();
        return true;
    }
    error(message);
    return false;
}

// Error handling
void Parser::error(const std::string& message) {
    std::ostringstream oss;
    if (!is_at_end()) {
        oss << "Error at position " << current_token().position << ": " << message;
    } else {
        oss << "Error at EOF: " << message;
    }
    errors_.push_back(oss.str());
}

void Parser::error_at_current(const std::string& message) {
    error(message);
}

void Parser::synchronize() {
    advance();
    while (!is_at_end()) {
        // Stop at definition boundaries
        if (check(TokenType::KEYWORD_QUERY) ||
            check(TokenType::KEYWORD_MUTATION) ||
            check(TokenType::KEYWORD_SUBSCRIPTION) ||
            check(TokenType::KEYWORD_FRAGMENT)) {
            return;
        }
        advance();
    }
}

std::string_view Parser::current_value() const {
    return current_token().value;
}

// Helper to check if current token can be used as a name (identifier or most keywords)
bool Parser::is_name_token() const {
    TokenType type = current_token().type;
    return type == TokenType::IDENTIFIER ||
           type == TokenType::KEYWORD_ON ||
           type == TokenType::KEYWORD_FRAGMENT ||
           type == TokenType::KEYWORD_TRUE ||
           type == TokenType::KEYWORD_FALSE ||
           type == TokenType::KEYWORD_NULL ||
           type == TokenType::KEYWORD_TYPE ||
           type == TokenType::KEYWORD_INPUT ||
           type == TokenType::KEYWORD_ENUM ||
           type == TokenType::KEYWORD_INTERFACE ||
           type == TokenType::KEYWORD_UNION ||
           type == TokenType::KEYWORD_DIRECTIVE ||
           type == TokenType::KEYWORD_SCALAR ||
           type == TokenType::KEYWORD_EXTEND ||
           type == TokenType::KEYWORD_IMPLEMENTS ||
           type == TokenType::KEYWORD_SCHEMA ||
           type == TokenType::KEYWORD_INT ||
           type == TokenType::KEYWORD_FLOAT ||
           type == TokenType::KEYWORD_STRING ||
           type == TokenType::KEYWORD_BOOLEAN ||
           type == TokenType::KEYWORD_ID;
}

// Main parsing
arena_ptr<Document> Parser::parse_document() {
    try {
        return parse_document_impl();
    } catch (const std::exception& e) {
        error(std::string("Exception during parsing: ") + e.what());
        return nullptr;
    }
}

arena_ptr<Document> Parser::parse_document_impl() {
    auto* doc = arena_.create<Document>();
    
    while (!is_at_end()) {
        try {
            auto def = parse_definition();
            doc->definitions.push_back(std::move(def));
        } catch (...) {
            synchronize();
        }
    }
    
    return arena_ptr<Document>(doc);
}

Definition Parser::parse_definition() {
    // Check for fragment definition
    if (check(TokenType::KEYWORD_FRAGMENT)) {
        return parse_fragment_definition();
    }
    
    // Otherwise, it's an operation definition
    return parse_operation_definition();
}

OperationType Parser::parse_operation_type() {
    if (match(TokenType::KEYWORD_QUERY)) return OperationType::QUERY;
    if (match(TokenType::KEYWORD_MUTATION)) return OperationType::MUTATION;
    if (match(TokenType::KEYWORD_SUBSCRIPTION)) return OperationType::SUBSCRIPTION;
    
    // Default to query for shorthand syntax
    return OperationType::QUERY;
}

arena_ptr<OperationDefinition> Parser::parse_operation_definition() {
    auto* op = arena_.create<OperationDefinition>();
    op->position = current_token().position;
    
    // Check for shorthand query (just selection set)
    if (check(TokenType::LEFT_BRACE)) {
        op->operation_type = OperationType::QUERY;
        op->name = "";
        op->selection_set = parse_selection_set();
        return arena_ptr<OperationDefinition>(op);
    }
    
    // Parse operation type
    op->operation_type = parse_operation_type();
    
    // Optional operation name
    if (check(TokenType::IDENTIFIER)) {
        op->name = current_value();
        advance();
    }
    
    // Optional variable definitions
    if (check(TokenType::LEFT_PAREN)) {
        op->variable_definitions = parse_variable_definitions();
    }
    
    // Optional directives
    op->directives = parse_directives();
    
    // Selection set (required)
    if (!check(TokenType::LEFT_BRACE)) {
        error("Expected selection set");
        return arena_ptr<OperationDefinition>(op);
    }
    op->selection_set = parse_selection_set();
    
    return arena_ptr<OperationDefinition>(op);
}

arena_ptr<FragmentDefinition> Parser::parse_fragment_definition() {
    auto* frag = arena_.create<FragmentDefinition>();
    frag->position = current_token().position;
    
    expect(TokenType::KEYWORD_FRAGMENT, "Expected 'fragment'");
    
    // Fragment name
    if (!check(TokenType::IDENTIFIER)) {
        error("Expected fragment name");
        return arena_ptr<FragmentDefinition>(frag);
    }
    frag->name = current_value();
    advance();
    
    // Type condition
    expect(TokenType::KEYWORD_ON, "Expected 'on' in fragment definition");
    
    if (!check(TokenType::IDENTIFIER)) {
        error("Expected type name");
        return arena_ptr<FragmentDefinition>(frag);
    }
    frag->type_condition = current_value();
    advance();
    
    // Optional directives
    frag->directives = parse_directives();
    
    // Selection set
    frag->selection_set = parse_selection_set();
    
    return arena_ptr<FragmentDefinition>(frag);
}

arena_ptr<SelectionSet> Parser::parse_selection_set() {
    auto* sel_set = arena_.create<SelectionSet>();
    sel_set->position = current_token().position;
    
    expect(TokenType::LEFT_BRACE, "Expected '{'");
    
    while (!check(TokenType::RIGHT_BRACE) && !is_at_end()) {
        size_t before = current_;
        sel_set->selections.push_back(parse_selection());
        
        // Skip optional comma
        match(TokenType::COMMA);
        
        // Safety: if we didn't advance, skip the problematic token
        if (current_ == before) {
            error("Unable to parse selection");
            advance();  // Force progress to avoid infinite loop
        }
    }
    
    expect(TokenType::RIGHT_BRACE, "Expected '}'");
    
    return arena_ptr<SelectionSet>(sel_set);
}

Selection Parser::parse_selection() {
    // Fragment spread: ...FragmentName
    if (check(TokenType::ELLIPSIS)) {
        advance();
        
        // Inline fragment: ...on Type
        if (check(TokenType::KEYWORD_ON)) {
            return parse_inline_fragment();
        }
        
        // Named fragment spread
        return parse_fragment_spread();
    }
    
    // Field
    return parse_field();
}

arena_ptr<Field> Parser::parse_field() {
    auto* field = arena_.create<Field>();
    field->position = current_token().position;
    
    if (!check(TokenType::IDENTIFIER)) {
        error("Expected field name");
        return arena_ptr<Field>(field);
    }
    
    // Check for alias (fieldName: actualField)
    std::string_view first_name = current_value();
    advance();
    
    if (match(TokenType::COLON)) {
        // First name was alias
        field->alias = first_name;
        if (!check(TokenType::IDENTIFIER)) {
            error("Expected field name after ':'");
            return arena_ptr<Field>(field);
        }
        field->name = current_value();
        advance();
    } else {
        // No alias
        field->name = first_name;
    }
    
    // Optional arguments
    if (check(TokenType::LEFT_PAREN)) {
        field->arguments = parse_arguments();
    }
    
    // Optional directives
    field->directives = parse_directives();
    
    // Optional selection set
    if (check(TokenType::LEFT_BRACE)) {
        field->selection_set = parse_selection_set();
    }
    
    return arena_ptr<Field>(field);
}

arena_ptr<FragmentSpread> Parser::parse_fragment_spread() {
    auto* spread = arena_.create<FragmentSpread>();
    spread->position = current_token().position;
    
    if (!check(TokenType::IDENTIFIER)) {
        error("Expected fragment name");
        return arena_ptr<FragmentSpread>(spread);
    }
    
    spread->name = current_value();
    advance();
    
    spread->directives = parse_directives();
    
    return arena_ptr<FragmentSpread>(spread);
}

arena_ptr<InlineFragment> Parser::parse_inline_fragment() {
    auto* frag = arena_.create<InlineFragment>();
    frag->position = current_token().position;
    
    // 'on' keyword already consumed
    advance();
    
    // Type condition
    if (check(TokenType::IDENTIFIER)) {
        frag->type_condition = current_value();
        advance();
    }
    
    frag->directives = parse_directives();
    frag->selection_set = parse_selection_set();
    
    return arena_ptr<InlineFragment>(frag);
}

// Arguments
std::vector<arena_ptr<Argument>> Parser::parse_arguments() {
    std::vector<arena_ptr<Argument>> args;
    
    expect(TokenType::LEFT_PAREN, "Expected '('");
    
    while (!check(TokenType::RIGHT_PAREN) && !is_at_end()) {
        size_t before = current_;
        args.push_back(parse_argument());
        
        // Skip optional comma
        match(TokenType::COMMA);
        
        // Safety: ensure progress
        if (current_ == before) {
            error("Unable to parse argument");
            advance();
        }
    }
    
    expect(TokenType::RIGHT_PAREN, "Expected ')'");
    
    return args;
}

arena_ptr<Argument> Parser::parse_argument() {
    auto* arg = arena_.create<Argument>();
    arg->position = current_token().position;
    
    // Argument names can be identifiers or keywords (GraphQL allows keywords as names)
    if (!is_name_token()) {
        error("Expected argument name");
        return arena_ptr<Argument>(arg);
    }
    
    arg->name = current_value();
    advance();
    
    expect(TokenType::COLON, "Expected ':' after argument name");
    
    arg->value = parse_value();
    
    return arena_ptr<Argument>(arg);
}

// Directives
std::vector<arena_ptr<Directive>> Parser::parse_directives() {
    std::vector<arena_ptr<Directive>> directives;
    
    while (check(TokenType::DIRECTIVE)) {
        directives.push_back(parse_directive());
    }
    
    return directives;
}

arena_ptr<Directive> Parser::parse_directive() {
    auto* dir = arena_.create<Directive>();
    dir->position = current_token().position;
    
    // Directive name (without @)
    std::string_view full_value = current_value();
    if (full_value.size() > 0 && full_value[0] == '@') {
        dir->name = full_value.substr(1);
    } else {
        dir->name = full_value;
    }
    advance();
    
    // Optional arguments
    if (check(TokenType::LEFT_PAREN)) {
        dir->arguments = parse_arguments();
    }
    
    return arena_ptr<Directive>(dir);
}

// Variables
std::vector<arena_ptr<VariableDefinition>> Parser::parse_variable_definitions() {
    std::vector<arena_ptr<VariableDefinition>> var_defs;
    
    expect(TokenType::LEFT_PAREN, "Expected '('");
    
    while (!check(TokenType::RIGHT_PAREN) && !is_at_end()) {
        size_t before = current_;
        var_defs.push_back(parse_variable_definition());
        
        // Skip optional comma
        match(TokenType::COMMA);
        
        // Safety: ensure progress
        if (current_ == before) {
            error("Unable to parse variable definition");
            advance();
        }
    }
    
    expect(TokenType::RIGHT_PAREN, "Expected ')'");
    
    return var_defs;
}

arena_ptr<VariableDefinition> Parser::parse_variable_definition() {
    auto* var_def = arena_.create<VariableDefinition>();
    var_def->position = current_token().position;
    
    // Variable
    var_def->variable = parse_variable();
    
    expect(TokenType::COLON, "Expected ':' after variable");
    
    // Type
    var_def->type = parse_type();
    
    // Default value
    if (match(TokenType::SYMBOL) && tokens_[current_ - 1].value == "=") {
        auto* default_val = arena_.create<Value>(parse_value()); var_def->default_value = arena_ptr<Value>(default_val);
    }
    
    // Directives
    var_def->directives = parse_directives();
    
    return arena_ptr<VariableDefinition>(var_def);
}

arena_ptr<Variable> Parser::parse_variable() {
    auto* var = arena_.create<Variable>();
    var->position = current_token().position;
    
    if (!check(TokenType::VARIABLE)) {
        error("Expected variable");
        return arena_ptr<Variable>(var);
    }
    
    // Variable name (without $)
    std::string_view full_value = current_value();
    if (full_value.size() > 0 && full_value[0] == '$') {
        var->name = full_value.substr(1);
    } else {
        var->name = full_value;
    }
    advance();
    
    return arena_ptr<Variable>(var);
}

// Types
arena_ptr<ASTNode> Parser::parse_type() {
    arena_ptr<ASTNode> type;
    
    if (check(TokenType::LEFT_BRACKET)) {
        type = parse_list_type();
    } else {
        type = parse_named_type();
    }
    
    // Check for non-null (!)
    if (match(TokenType::EXCLAMATION)) {
        NonNullType nnt;
        nnt.type = std::move(type);
        nnt.position = current_token().position;
        auto* node = arena_.create<ASTNode>(std::move(nnt));
        return arena_ptr<ASTNode>(node);
    }
    
    return type;
}

arena_ptr<ASTNode> Parser::parse_named_type() {
    NamedType nt;
    nt.position = current_token().position;
    
    if (!check(TokenType::IDENTIFIER)) {
        error("Expected type name");
    } else {
        nt.name = current_value();
        advance();
    }
    
    auto* node = arena_.create<ASTNode>(std::move(nt));
    return arena_ptr<ASTNode>(node);
}

arena_ptr<ASTNode> Parser::parse_list_type() {
    ListType lt;
    lt.position = current_token().position;
    
    expect(TokenType::LEFT_BRACKET, "Expected '['");
    lt.type = parse_type();
    expect(TokenType::RIGHT_BRACKET, "Expected ']'");
    
    auto* node = arena_.create<ASTNode>(std::move(lt));
    return arena_ptr<ASTNode>(node);
}

// Values
Value Parser::parse_value() {
    if (check(TokenType::VARIABLE)) {
        auto var = parse_variable();
        // Transfer ownership - var is already arena_ptr, just move it
        return std::move(var);
    }
    
    if (check(TokenType::NUMBER)) {
        // Check if it's a float or int
        std::string_view val = current_value();
        if (val.find('.') != std::string_view::npos ||
            val.find('e') != std::string_view::npos ||
            val.find('E') != std::string_view::npos) {
            return parse_float_value();
        }
        return parse_int_value();
    }
    
    if (check(TokenType::STRING)) {
        return parse_string_value();
    }
    
    if (check(TokenType::KEYWORD_TRUE) || check(TokenType::KEYWORD_FALSE)) {
        return parse_boolean_value();
    }
    
    if (check(TokenType::KEYWORD_NULL)) {
        return parse_null_value();
    }
    
    if (check(TokenType::LEFT_BRACKET)) {
        return parse_list_value();
    }
    
    if (check(TokenType::LEFT_BRACE)) {
        return parse_object_value();
    }
    
    // Enum value (identifier)
    if (check(TokenType::IDENTIFIER)) {
        return parse_enum_value();
    }
    
    error("Expected value");
    return NullValue{current_token().position};
}

Value Parser::parse_int_value() {
    IntValue iv;
    iv.value = current_value();
    iv.position = current_token().position;
    advance();
    return iv;
}

Value Parser::parse_float_value() {
    FloatValue fv;
    fv.value = current_value();
    fv.position = current_token().position;
    advance();
    return fv;
}

Value Parser::parse_string_value() {
    StringValue sv;
    sv.value = current_value();
    sv.position = current_token().position;
    advance();
    return sv;
}

Value Parser::parse_boolean_value() {
    BooleanValue bv;
    bv.value = check(TokenType::KEYWORD_TRUE);
    bv.position = current_token().position;
    advance();
    return bv;
}

Value Parser::parse_null_value() {
    NullValue nv;
    nv.position = current_token().position;
    advance();
    return nv;
}

Value Parser::parse_enum_value() {
    EnumValue ev;
    ev.value = current_value();
    ev.position = current_token().position;
    advance();
    return ev;
}

Value Parser::parse_list_value() {
    auto* lv = arena_.create<ListValue>();
    lv->position = current_token().position;
    
    expect(TokenType::LEFT_BRACKET, "Expected '['");
    
    while (!check(TokenType::RIGHT_BRACKET) && !is_at_end()) {
        size_t before = current_;
        lv->values.push_back(parse_value());
        
        // Skip optional comma
        match(TokenType::COMMA);
        
        // Safety: ensure progress
        if (current_ == before) {
            error("Unable to parse list value");
            advance();
        }
    }
    
    expect(TokenType::RIGHT_BRACKET, "Expected ']'");
    
    return arena_ptr<ListValue>(lv);
}

Value Parser::parse_object_value() {
    auto* ov = arena_.create<ObjectValue>();
    ov->position = current_token().position;
    
    expect(TokenType::LEFT_BRACE, "Expected '{'");
    
    while (!check(TokenType::RIGHT_BRACE) && !is_at_end()) {
        size_t before = current_;
        ObjectField field;
        field.position = current_token().position;
        
        // Object field names can be identifiers or keywords
        if (!is_name_token()) {
            error("Expected field name");
            advance();  // Skip problematic token
            continue;
        }
        
        field.name = current_value();
        advance();
        
        expect(TokenType::COLON, "Expected ':'");
        
        field.value = parse_value();
        
        ov->fields.push_back(std::move(field));
        
        // Skip optional comma
        match(TokenType::COMMA);
        
        // Safety: ensure progress
        if (current_ == before) {
            error("Unable to parse object field");
            advance();
        }
    }
    
    expect(TokenType::RIGHT_BRACE, "Expected '}'");
    
    return arena_ptr<ObjectValue>(ov);
}

