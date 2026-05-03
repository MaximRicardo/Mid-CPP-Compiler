#include "func_decl.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/end_types.h"
#include "parser/expr.h"
#include "parser/find_twin.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "print.h"
#include <string.h>

struct Diag expected_token(const char *name, const struct Lexer_Token *tok,
                           enum ErrorType err_type)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("expected '%s'", name),
        .err = err_type,
        .is_err = true,
    };
}

void Parser_FuncDecl_deinit(struct Parser_FuncDecl *self)
{
    Parser_Type_deinit(&self->type);
    gen_dyndeinit(&self->params, Parser_VarDecl_deinit);
    gen_dyndeinit(&self->nodes, Parser_ASTNodeP_deinit);
}

struct Parser_VarDeclVec
Parser_parse_func_params(const struct Lexer_Token *toks, isize_t lparen,
                         isize_t *out_rparen, struct Parser_ASTNode *node,
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

    for (isize_t i = lparen + 1; i < rparen; ++i) {
        struct Parser_VarDecl child = {};
        i = Parser_parse_var_decl(toks, i, PARSER_PARAM_ENDTYPES, &child, node,
                                  false, diags);
        gen_dynpush(&params, child);
    }

    return params;
}

isize_t Parser_parse_func_body(const struct Lexer_Token *toks, isize_t lcurly,
                               struct Parser_FuncDecl *decl,
                               struct Parser_ASTNode *node,
                               struct DiagVec *diags)
{
    isize_t rcurly = Parser_find_twin_curly(toks, lcurly, ISIZE_MAX);
    if (rcurly == -1)
        rcurly = lcurly;

    if (rcurly == -1) {
        gen_dynpush(diags,
                    ((struct Diag){.pos = toks[lcurly].pos,
                                   .line = toks[lcurly].line,
                                   .msg = Print_fmt_to_str("expected '}'"),
                                   .err = ERRORTYPE_MISSING_CURLY,
                                   .is_err = true}));
        return rcurly;
    }

    for (isize_t i = lcurly + 1; i < rcurly;) {
        auto child = Parser_parse_node(toks, i, &i, node, false, diags);
        gen_dynpush(&decl->nodes, child);
    }

    return rcurly;
}

// op is the index of the operator
// Type operator+(const Type &a, const Type &b)
//              ^
//              op
// NOTE: can't capture information dependent on other stuff, like whether an
//       increment overload is postfix or prefix. assumes postfix by default and
//       binary operators instead of unary operators by default for stuff like
//       '+' and '-'
static enum Parser_ExprType
parse_operator_overload(const struct Lexer_Token *toks, isize_t op,
                        isize_t *out_end, struct DiagVec *diags)
{
    if (out_end)
        *out_end = op + 1;

    switch (toks[op].type) {
    case LEXER_TOKENTYPE_ADD:
        return PARSER_EXPRTYPE_ADD;

    case LEXER_TOKENTYPE_SUB:
        return PARSER_EXPRTYPE_SUB;

    case LEXER_TOKENTYPE_MUL:
        return PARSER_EXPRTYPE_MUL;

    case LEXER_TOKENTYPE_DIV:
        return PARSER_EXPRTYPE_DIV;

    case LEXER_TOKENTYPE_MOD:
        return PARSER_EXPRTYPE_MOD;

    case LEXER_TOKENTYPE_INC:
        return PARSER_EXPRTYPE_POSTFIX_INC;

    case LEXER_TOKENTYPE_DEC:
        return PARSER_EXPRTYPE_POSTFIX_DEC;

    case LEXER_TOKENTYPE_EQ:
        return PARSER_EXPRTYPE_EQ;

    case LEXER_TOKENTYPE_NEQ:
        return PARSER_EXPRTYPE_NEQ;

    case LEXER_TOKENTYPE_GT:
        return PARSER_EXPRTYPE_GT;

    case LEXER_TOKENTYPE_LT:
        return PARSER_EXPRTYPE_LT;

    case LEXER_TOKENTYPE_GTEQ:
        return PARSER_EXPRTYPE_GTEQ;

    case LEXER_TOKENTYPE_LTEQ:
        return PARSER_EXPRTYPE_LTEQ;

    case LEXER_TOKENTYPE_LOGICAL_NOT:
        return PARSER_EXPRTYPE_LOGICAL_NOT;

    case LEXER_TOKENTYPE_LOGICAL_AND:
        return PARSER_EXPRTYPE_LOGICAL_AND;

    case LEXER_TOKENTYPE_LOGICAL_OR:
        return PARSER_EXPRTYPE_LOGICAL_OR;

    case LEXER_TOKENTYPE_BITWISE_NOT:
        return PARSER_EXPRTYPE_BITWISE_NOT;

    case LEXER_TOKENTYPE_BITWISE_AND:
        return PARSER_EXPRTYPE_BITWISE_AND;

    case LEXER_TOKENTYPE_BITWISE_OR:
        return PARSER_EXPRTYPE_BITWISE_OR;

    case LEXER_TOKENTYPE_BITWISE_XOR:
        return PARSER_EXPRTYPE_BITWISE_XOR;

    case LEXER_TOKENTYPE_LEFT_SHIFT:
        return PARSER_EXPRTYPE_LEFT_SHIFT;

    case LEXER_TOKENTYPE_RIGHT_SHIFT:
        return PARSER_EXPRTYPE_RIGHT_SHIFT;

    case LEXER_TOKENTYPE_ASSIGN:
        return PARSER_EXPRTYPE_ASSIGN;

    case LEXER_TOKENTYPE_ADD_ASSIGN:
        return PARSER_EXPRTYPE_ADD_ASSIGN;

    case LEXER_TOKENTYPE_SUB_ASSIGN:
        return PARSER_EXPRTYPE_SUB_ASSIGN;

    case LEXER_TOKENTYPE_MUL_ASSIGN:
        return PARSER_EXPRTYPE_MUL_ASSIGN;

    case LEXER_TOKENTYPE_DIV_ASSIGN:
        return PARSER_EXPRTYPE_DIV_ASSIGN;

    case LEXER_TOKENTYPE_MOD_ASSIGN:
        return PARSER_EXPRTYPE_MOD_ASSIGN;

    case LEXER_TOKENTYPE_AND_ASSIGN:
        return PARSER_EXPRTYPE_AND_ASSIGN;

    case LEXER_TOKENTYPE_OR_ASSIGN:
        return PARSER_EXPRTYPE_OR_ASSIGN;

    case LEXER_TOKENTYPE_XOR_ASSIGN:
        return PARSER_EXPRTYPE_XOR_ASSIGN;

    case LEXER_TOKENTYPE_LEFT_SHIFT_ASSIGN:
        return PARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN;

    case LEXER_TOKENTYPE_RIGHT_SHIFT_ASSIGN:
        return PARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN;

    case LEXER_TOKENTYPE_L_SQBRACKET:
        if (toks[op + 1].type != LEXER_TOKENTYPE_R_SQBRACKET)
            gen_dynpush(diags, expected_token("]", &toks[op],
                                              ERRORTYPE_BAD_OP_OVERLOAD));
        else if (out_end)
            ++*out_end;
        return PARSER_EXPRTYPE_ARRAY_SUBSCR;

    case LEXER_TOKENTYPE_PTR_MEMB_SEL:
        return PARSER_EXPRTYPE_PTR_MEMB_SEL;

    case LEXER_TOKENTYPE_PTR_TO_PTR_MEMB_SEL:
        return PARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;

    case LEXER_TOKENTYPE_L_PAREN:
        if (toks[op + 1].type != LEXER_TOKENTYPE_R_PAREN)
            gen_dynpush(diags, expected_token(")", &toks[op],
                                              ERRORTYPE_BAD_OP_OVERLOAD));
        else if (out_end)
            ++*out_end;
        return PARSER_EXPRTYPE_FUNC_CALL;

    case LEXER_TOKENTYPE_COMMA:
        return PARSER_EXPRTYPE_COMMA;

    case LEXER_TOKENTYPE_NEW:
        if (toks[op + 1].type == LEXER_TOKENTYPE_L_SQBRACKET) {
            if (out_end)
                ++*out_end;

            if (toks[op + 2].type != LEXER_TOKENTYPE_R_SQBRACKET)
                gen_dynpush(diags, expected_token("]", &toks[op + 1],
                                                  ERRORTYPE_BAD_OP_OVERLOAD));
            else if (out_end)
                ++*out_end;
            return PARSER_EXPRTYPE_NEW_ARR;
        } else {
            return PARSER_EXPRTYPE_NEW;
        }

    case LEXER_TOKENTYPE_DELETE:
        if (toks[op + 1].type == LEXER_TOKENTYPE_L_SQBRACKET) {
            if (out_end)
                ++*out_end;

            if (toks[op + 2].type != LEXER_TOKENTYPE_R_SQBRACKET)
                gen_dynpush(diags, expected_token("]", &toks[op + 1],
                                                  ERRORTYPE_BAD_OP_OVERLOAD));
            else if (out_end)
                ++*out_end;
            return PARSER_EXPRTYPE_DELETE_ARR;
        } else {
            return PARSER_EXPRTYPE_DELETE;
        }

    default:
        gen_dynpush(diags, ((struct Diag){
                               .pos = toks[op].pos,
                               .line = toks[op].line,
                               .msg = strdup("can't overload operator"),
                               .err = ERRORTYPE_BAD_OP_OVERLOAD,
                               .is_err = true,
                           }));
        if (out_end)
            --*out_end;
        // just default to add for now
        return PARSER_EXPRTYPE_ADD;
    }
}

static void parse_func_type(struct Parser_FuncDecl *decl,
                            const struct Lexer_Token *toks, isize_t start,
                            isize_t *out_end, struct Parser_ASTNode *node,
                            struct DiagVec *diags)
{
    isize_t type_end;
    decl->type = Parser_parse_type(toks, start, &type_end, node->parent,
                                   &decl->name, diags);

    if (!decl->name) {
        gen_dynpush(diags, expected_token("identifier", &toks[start],
                                          ERRORTYPE_MISSING_TOKEN));
        decl->name = "INVALID-FUNC-NAME";
    } else if (!strcmp(decl->name, "operator")) {
        decl->is_op_overload = true;
        decl->op_overload =
            parse_operator_overload(toks, type_end, &type_end, diags);
    }

    if (out_end)
        *out_end = type_end;
}

// some operator overloads are ambiguous until the parameters have been parsed
static void disambig_operator_overload(struct Parser_FuncDecl *decl)
{
    assert(decl->is_op_overload);

    if (decl->params.len == 1) {
        switch (decl->op_overload) {
        case PARSER_EXPRTYPE_ADD:
            decl->op_overload = PARSER_EXPRTYPE_UNARY_PLUS;
            break;

        case PARSER_EXPRTYPE_SUB:
            decl->op_overload = PARSER_EXPRTYPE_UNARY_MINUS;
            break;

        case PARSER_EXPRTYPE_MUL:
            decl->op_overload = PARSER_EXPRTYPE_DEREF;
            break;

        case PARSER_EXPRTYPE_BITWISE_AND:
            decl->op_overload = PARSER_EXPRTYPE_REF;
            break;

        case PARSER_EXPRTYPE_POSTFIX_INC:
            decl->op_overload = PARSER_EXPRTYPE_PREFIX_INC;
            break;

        case PARSER_EXPRTYPE_POSTFIX_DEC:
            decl->op_overload = PARSER_EXPRTYPE_PREFIX_DEC;
            break;

        default:
            break;
        }
    }
}

isize_t Parser_parse_func_decl(const struct Lexer_Token *toks, isize_t start,
                               struct Parser_FuncDecl *decl,
                               struct Parser_ASTNode *node, bool skip_def,
                               struct DiagVec *diags)
{
    *decl = (struct Parser_FuncDecl){};

    isize_t type_end;
    parse_func_type(decl, toks, start, &type_end, node, diags);

    if (toks[type_end].type != LEXER_TOKENTYPE_L_PAREN)
        CRASH("function missing left paren");

    isize_t lparen = type_end;
    isize_t rparen;
    decl->params =
        Parser_parse_func_params(toks, lparen, &rparen, node->parent, diags);
    if (decl->is_op_overload)
        disambig_operator_overload(decl);

    isize_t lcurly = rparen + 1;
    printf("lcurly at %d:%d\n", toks[lcurly].pos.line, toks[lcurly].pos.column);
    if (toks[lcurly].type != LEXER_TOKENTYPE_L_CURLY)
        return lcurly;
    decl->def_start = &toks[lcurly];
    decl->has_def = true;

    if (skip_def) {
        isize_t rcurly = Parser_find_twin_curly(toks, lcurly, ISIZE_MAX);
        return rcurly == -1 ? lcurly + 1 : rcurly + 1;
    } else {
        isize_t rcurly =
            Parser_parse_func_body(toks, lcurly, decl, node, diags);
        return rcurly + 1;
    }
}
