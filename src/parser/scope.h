#pragma once

#include "ast.h"
#include "generics/dynarray.h"

enum Parser_ScopeType {
    PARSER_SCOPETYPE_GLOBAL,
    PARSER_SCOPETYPE_BLOCK,
    PARSER_SCOPETYPE_FUNC_PARAM,
    PARSER_SCOPETYPE_FUNC_DEF,
    PARSER_SCOPETYPE_TEMPLATE_PARAM,
    PARSER_SCOPETYPE_LAMBDA,
    PARSER_SCOPETYPE_NAMESPACE,
    PARSER_SCOPETYPE_CLASS,
    PARSER_SCOPETYPE_ENUM,
};

gen_dynarray_struct_named(Parser_ScopeVec, struct Parser_Scope);
struct Parser_Scope {
    const struct Lexer_Token *start;
    struct Parser_ASTNode *parent;
    struct Parser_ASTNodeVec nodes; // children but that's too long a name
    const char *name;               // NOTE: not all scopes have names
    enum Parser_ScopeType type;
};
