#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "sema/scope.h"

// classes, structs and unions
struct Parser_Class {
    struct Parser_ASTNodePVec nodes;
    const char *name;
    struct Sema_Scope *scope;
    struct Parser_ASTNode *super;        // class this class inherits from
    const struct Lexer_Token *def_start; // the left curly '{'
    bool is_union;
    bool has_def;
};

void Parser_Class_deinit(struct Parser_Class *self);
// returns the end of the class
isize_t Parser_parse_class(struct Parser_Class *self,
                           struct Parser_ASTNode *node,
                           struct Sema_Scope *scope,
                           const struct Lexer_Token *toks, isize_t start,
                           bool skip_def, struct Parser_Allocators *allocs,
                           struct DiagVec *diags);
// returns the end of the class body
isize_t Parser_parse_class_body(struct Parser_Class *self,
                                struct Parser_ASTNode *node,
                                struct Sema_Scope *scope,
                                const struct Lexer_Token *toks, isize_t l_curly,
                                struct Parser_Allocators *allocs,
                                struct DiagVec *diags);
