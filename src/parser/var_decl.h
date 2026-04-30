#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "parser/expr.h"
#include "type.h"

struct Parser_VarDecl {
    struct Parser_Type type;
    const char *name;
    struct Parser_Expr *init;
};
gen_dynarray_struct_named(Parser_VarDeclVec, struct Parser_VarDecl);

void Parser_VarDecl_deinit(struct Parser_VarDecl *self);
struct Parser_VarDecl Parser_parse_var_decl(const struct Lexer_Token *toks,
                                            isize_t start, isize_t *out_end,
                                            struct Parser_ASTNode *parent,
                                            struct DiagVec *diags);
