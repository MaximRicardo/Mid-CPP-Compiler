#pragma once

#include "allocator.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/expr.h"
#include "sema/scope.h"
#include "type.h"

struct Parser_VarDecl {
    struct Parser_Type type;
    const char *name;
    struct Parser_Expr *init;
    const struct Lexer_Token *init_start; // points to the first token of the
                                          // initialization expr
};
gen_dynarray_struct_named(Parser_VarDeclVec, struct Parser_VarDecl);

void Parser_VarDecl_deinit(struct Parser_VarDecl *self);
// add_to_scope - if true, the variable is added as an identifier in the passed
//                scope.
// skip_init - if true, the var initializer won't be parsed, but init_start will
//             still be set to the first token of the initializer if there is
//             one. used by classes, which parse declarations first then
//             definitions
isize_t Parser_parse_var_decl(const struct Lexer_Token *toks, isize_t start,
                              const enum Lexer_TokenType *end_types,
                              isize_t n_end_types, struct Parser_VarDecl *decl,
                              struct Parser_ASTNode *node,
                              struct Sema_Scope *scope, bool add_to_scope,
                              bool skip_init, struct Parser_Allocators *allocs,
                              struct DiagVec *diags);
