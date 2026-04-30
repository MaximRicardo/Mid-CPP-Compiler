#include "func_decl.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/find_twin.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "print.h"

void Parser_FuncDecl_deinit(struct Parser_FuncDecl *self)
{
    Parser_Type_deinit(&self->type);
    gen_dyndeinit(&self->params, Parser_VarDecl_deinit);
    gen_dyndeinit(&self->nodes, Parser_ASTNode_deinit);
}

struct Parser_VarDeclVec
Parser_parse_func_params(const struct Lexer_Token *toks, isize_t lparen,
                         isize_t *out_rparen, struct Parser_ASTNode *parent,
                         struct DiagVec *diags)
{
    struct Parser_VarDeclVec params = {};

    isize_t rparen = Parser_find_twin_paren(toks, lparen, ISIZE_MAX);
    if (out_rparen)
        *out_rparen = rparen;

    if (rparen == -1) {
        gen_dynpush(diags,
                    ((struct Diag){.pos = toks[lparen].pos,
                                   .line = toks[lparen].line,
                                   .msg = Print_fmt_to_str("expected ')'"),
                                   .err = ERRORTYPE_MISSING_PAREN,
                                   .is_err = true}));
        return params;
    }

    for (isize_t i = lparen + 1; i < rparen; ++i)
        gen_dynpush(&params, Parser_parse_var_decl(toks, i, &i, parent, diags));

    return params;
}

static void parse_func_body(struct Parser_FuncDecl *func,
                            const struct Lexer_Token *toks, isize_t lcurly,
                            isize_t *out_rcurly,
                            struct Parser_ASTNode *func_node,
                            struct DiagVec *diags)
{
    isize_t rcurly = Parser_find_twin_curly(toks, lcurly, ISIZE_MAX);
    if (out_rcurly)
        *out_rcurly = rcurly;

    if (rcurly == -1) {
        gen_dynpush(diags,
                    ((struct Diag){.pos = toks[lcurly].pos,
                                   .line = toks[lcurly].line,
                                   .msg = Print_fmt_to_str("expected '}'"),
                                   .err = ERRORTYPE_MISSING_CURLY,
                                   .is_err = true}));
        return;
    }

    for (isize_t i = lcurly + 1; i < rcurly;)
        gen_dynpush(&func->nodes,
                    Parser_parse_node(toks, i, &i, func_node, diags));
}

void Parser_parse_func_decl(const struct Lexer_Token *toks, isize_t start,
                            isize_t *out_end, struct Parser_ASTNode *func_node,
                            struct DiagVec *diags)
{
    struct Parser_FuncDecl *decl = &func_node->func_decl;

    isize_t type_end;
    decl->type = Parser_parse_type(toks, start, &type_end, func_node->parent,
                                   &decl->name, diags);

    if (toks[type_end].type != LEXER_TOKENTYPE_L_PAREN)
        CRASH("function missing left paren");

    isize_t lparen = type_end;
    isize_t rparen;
    decl->params = Parser_parse_func_params(toks, lparen, &rparen,
                                            func_node->parent, diags);

    isize_t lcurly = rparen + 1;
    if (toks[lcurly].type != LEXER_TOKENTYPE_L_CURLY) {
        if (out_end)
            *out_end = lcurly;
        return;
    }

    isize_t rcurly;
    parse_func_body(decl, toks, lcurly, &rcurly, func_node, diags);
    decl->has_def = true;

    if (out_end)
        *out_end = rcurly + 1;
    return;
}
