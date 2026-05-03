#pragma once

#include "ints.h"
#include "lexer/token.h"
#include "parser/astvec.h"
#include "parser/type.h"
#include "parser/var_decl.h"

struct Parser_FuncDecl {
    struct Parser_Type type;
    struct Parser_VarDeclVec params;
    struct Parser_ASTNodePVec nodes;
    const char *name;
    const struct Lexer_Token *def_start; // points to the left curly '{'
    enum Parser_ExprType op_overload;    // the operator that got overloaded
    bool is_op_overload;
    bool has_def; // does this node hold the definition of the func?
};

void Parser_FuncDecl_deinit(struct Parser_FuncDecl *self);
struct Parser_VarDeclVec
Parser_parse_func_params(const struct Lexer_Token *toks, isize_t lparen,
                         isize_t *out_rparen, struct Parser_ASTNode *node,
                         struct DiagVec *diags);
// skip_def -  if true, the func definition won't be parsed, but def_start will
//             still be set to the first token of the definition and has_def
//             will still be set to true if there is a definition. used by
//             classes, which parse declarations first then definitions
isize_t Parser_parse_func_decl(const struct Lexer_Token *toks, isize_t start,
                               struct Parser_FuncDecl *decl,
                               struct Parser_ASTNode *node, bool skip_def,
                               struct DiagVec *diags);
// returns the idx of the closing curly bracket
isize_t Parser_parse_func_body(const struct Lexer_Token *toks, isize_t lcurly,
                               struct Parser_FuncDecl *decl,
                               struct Parser_ASTNode *node,
                               struct DiagVec *diags);
