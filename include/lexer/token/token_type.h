#pragma once

#include <string>
#include <vector>

enum TokenType {
    KEYWORD_QUERY,
    KEYWORD_MUTATION,
    KEYWORD_SUBSCRIPTION,
    KEYWORD_FRAGMENT,
    KEYWORD_ON,
    KEYWORD_TRUE,
    KEYWORD_FALSE,
    KEYWORD_NULL,
    KEYWORD_TYPE,
    KEYWORD_INPUT,
    KEYWORD_ENUM,
    KEYWORD_INTERFACE,
    KEYWORD_UNION,
    KEYWORD_DIRECTIVE,
    KEYWORD_SCALAR,
    KEYWORD_EXTEND,
    KEYWORD_IMPLEMENTS,

    // Introspection
    KEYWORD_TYPENAME,
    KEYWORD_SCHEMA,
    KEYWORD_TYPE_META,
    KEYWORD_GET,
    KEYWORD_CREATE,
    KEYWORD_UPDATE,
    KEYWORD_DELETE,
    // GraphQL types
    KEYWORD_INT,
    KEYWORD_FLOAT,
    KEYWORD_STRING,
    KEYWORD_BOOLEAN,
    KEYWORD_ID,

    IDENTIFIER,
    VARIABLE,
    DIRECTIVE,

    NUMBER,
    STRING,
    SYMBOL,
    // Special characters with dedicated token types
    LEFT_BRACE,      // {
    RIGHT_BRACE,     // }
    LEFT_PAREN,      // (
    RIGHT_PAREN,     // )
    LEFT_BRACKET,    // [
    RIGHT_BRACKET,   // ]
    COLON,           // :
    COMMA,           // ,
    ELLIPSIS,        // ...
    EXCLAMATION,      // !
    
    // TODO: 
    BOOLEAN,
    NULL_VALUE,

    UNKNOWN
};
