#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/astvec.h"

// classes, structs and unions
struct Parser_Class {
    struct Parser_ASTNodePVec nodes;
    const char *name;
    struct Parser_ASTNode *super; // class this class inherits from
    bool is_union;
    bool has_def;
};

void Parser_Class_deinit(struct Parser_Class *self);
isize_t Parser_parse_class(struct Parser_ASTNode *node,
                           const struct Lexer_Token *toks, isize_t start,
                           struct DiagVec *diags);
