#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <variant>
#include "ast/ast_arena.h"  // For arena_ptr

// Forward declarations
struct ASTNode;
struct Document;
struct OperationDefinition;
struct FragmentDefinition;
struct SelectionSet;
struct Field;
struct Argument;
struct FragmentSpread;
struct InlineFragment;
struct Variable;
struct Directive;
struct NamedType;
struct ListType;
struct NonNullType;

// Base node type
enum class ASTNodeType {
    DOCUMENT,
    OPERATION_DEFINITION,
    FRAGMENT_DEFINITION,
    SELECTION_SET,
    FIELD,
    ARGUMENT,
    FRAGMENT_SPREAD,
    INLINE_FRAGMENT,
    VARIABLE,
    DIRECTIVE,
    NAMED_TYPE,
    LIST_TYPE,
    NON_NULL_TYPE,
    INT_VALUE,
    FLOAT_VALUE,
    STRING_VALUE,
    BOOLEAN_VALUE,
    NULL_VALUE,
    ENUM_VALUE,
    LIST_VALUE,
    OBJECT_VALUE
};

// Value types
struct IntValue {
    std::string_view value;
    size_t position;
};

struct FloatValue {
    std::string_view value;
    size_t position;
};

struct StringValue {
    std::string_view value;
    size_t position;
};

struct BooleanValue {
    bool value;
    size_t position;
};

struct NullValue {
    size_t position;
};

struct EnumValue {
    std::string_view value;
    size_t position;
};

struct ListValue;
struct ObjectValue;

using Value = std::variant<
    IntValue,
    FloatValue,
    StringValue,
    BooleanValue,
    NullValue,
    EnumValue,
    arena_ptr<ListValue>,
    arena_ptr<ObjectValue>,
    arena_ptr<Variable>
>;

struct ListValue {
    std::vector<Value> values;
    size_t position;
};

struct ObjectField {
    std::string_view name;
    Value value;
    size_t position;
};

struct ObjectValue {
    std::vector<ObjectField> fields;
    size_t position;
};

// Variable definition
struct Variable {
    std::string_view name;  // Without the $
    size_t position;
};

struct VariableDefinition {
    arena_ptr<Variable> variable;
    arena_ptr<ASTNode> type;  // Type reference (NamedType, ListType, NonNullType)
    arena_ptr<Value> default_value;
    std::vector<arena_ptr<Directive>> directives;
    size_t position;
};

// Type system
struct NamedType {
    std::string_view name;
    size_t position;
};

struct ListType {
    arena_ptr<ASTNode> type;
    size_t position;
};

struct NonNullType {
    arena_ptr<ASTNode> type;  // NamedType or ListType
    size_t position;
};

// Directive
struct Directive {
    std::string_view name;  // Without the @
    std::vector<arena_ptr<Argument>> arguments;
    size_t position;
};

// Argument
struct Argument {
    std::string_view name;
    Value value;
    size_t position;
};

// Selection types
struct Field {
    std::string_view alias;  // Optional, empty if no alias
    std::string_view name;
    std::vector<arena_ptr<Argument>> arguments;
    std::vector<arena_ptr<Directive>> directives;
    arena_ptr<SelectionSet> selection_set;  // Optional
    size_t position;
};

struct FragmentSpread {
    std::string_view name;
    std::vector<arena_ptr<Directive>> directives;
    size_t position;
};

struct InlineFragment {
    std::string_view type_condition;  // Optional, empty if no type condition
    std::vector<arena_ptr<Directive>> directives;
    arena_ptr<SelectionSet> selection_set;
    size_t position;
};

using Selection = std::variant<
    arena_ptr<Field>,
    arena_ptr<FragmentSpread>,
    arena_ptr<InlineFragment>
>;

struct SelectionSet {
    std::vector<Selection> selections;
    size_t position;
};

// Operation types
enum class OperationType {
    QUERY,
    MUTATION,
    SUBSCRIPTION
};

struct OperationDefinition {
    OperationType operation_type;
    std::string_view name;  // Optional, empty for anonymous
    std::vector<arena_ptr<VariableDefinition>> variable_definitions;
    std::vector<arena_ptr<Directive>> directives;
    arena_ptr<SelectionSet> selection_set;
    size_t position;
};

// Fragment definition
struct FragmentDefinition {
    std::string_view name;
    std::string_view type_condition;
    std::vector<arena_ptr<Directive>> directives;
    arena_ptr<SelectionSet> selection_set;
    size_t position;
};

// Document (root)
using Definition = std::variant<
    arena_ptr<OperationDefinition>,
    arena_ptr<FragmentDefinition>
>;

struct Document {
    std::vector<Definition> definitions;
};

// Base node for type references
struct ASTNode {
    ASTNodeType type;
    std::variant<NamedType, ListType, NonNullType> data;
    
    explicit ASTNode(NamedType nt) : type(ASTNodeType::NAMED_TYPE), data(std::move(nt)) {}
    explicit ASTNode(ListType lt) : type(ASTNodeType::LIST_TYPE), data(std::move(lt)) {}
    explicit ASTNode(NonNullType nnt) : type(ASTNodeType::NON_NULL_TYPE), data(std::move(nnt)) {}
};

