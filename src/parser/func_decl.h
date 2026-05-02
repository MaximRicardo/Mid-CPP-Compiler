#pragma once

#include "parser/astvec.h"
#include "parser/type.h"
#include "parser/var_decl.h"

struct Parser_FuncDecl {
    struct Parser_Type type;
    struct Parser_VarDeclVec params;
    struct Parser_ASTNodePVec nodes;
    const char *name;
    enum Parser_ExprType op_overload; // the operator that got overloaded
    bool is_op_overload;
    bool has_def; // does this node hold the definition of the func?
};

void Parser_FuncDecl_deinit(struct Parser_FuncDecl *self);
struct Parser_VarDeclVec
Parser_parse_func_params(const struct Lexer_Token *toks, isize_t lparen,
                         isize_t *out_rparen, struct Parser_ASTNode *func_node,
                         struct DiagVec *diags);
void Parser_parse_func_decl(const struct Lexer_Token *toks, isize_t start,
                            isize_t *out_end, struct Parser_ASTNode *func_node,
                            struct DiagVec *diags);
