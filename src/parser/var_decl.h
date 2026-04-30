#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/expr.h"
#include "type.h"

struct Parser_VarDecl {
    struct Parser_Type type;
    const char *name;
    struct Parser_Expr *init;
};
gen_dynarray_struct_named(Parser_VarDeclVec, struct Parser_VarDecl);

void Parser_VarDecl_deinit(struct Parser_VarDecl *self);
// is_param - if true, the decl will stop when it encounters either a comma or
//            a left parenthesis. otherwise stops on semicolon
struct Parser_VarDecl
Parser_parse_var_decl(const struct Lexer_Token *toks, isize_t start,
                      isize_t *out_end, const enum Lexer_TokenType *end_types,
                      isize_t n_end_types, const struct Parser_ASTNode *parent,
                      struct DiagVec *diags);
