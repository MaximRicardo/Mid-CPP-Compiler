#include "func_decl.h"
#include "diag.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/astvec.h"
#include "parser/end_types.h"
#include "parser/expr.h"
#include "parser/find_twin.h"
#include "parser/scope.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "print.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include <string.h>

static struct Diag missing_default_arg_err(const char *func,
                                           const struct Lexer_Token *tok,
                                           enum ErrorType err_type)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("function '%s' missing default argument", func),
        .err = err_type,
        .is_err = true,
    };
}

void Parser_FuncDecl_deinit(struct Parser_FuncDecl *self)
{
    gen_dyndeinit(&self->nodes);
    gen_dyndeinit(&self->params);
    Parser_Type_deinit(&self->type);
}

struct Sema_Scope *Parser_func_parent(const struct Parser_FuncDecl *func)
{
    auto ret = func->param_scope->parent;
    assert(ret);
    return ret;
}

struct Sema_Ident *Parser_func_ident(const struct Parser_FuncDecl *func)
{
    assert(func->ident_idx != -1);
    auto parent = Parser_func_parent(func);
    return &parent->idents.arr[func->ident_idx];
}

struct Parser_ASTNodePVec Parser_parse_func_params(
    const struct Lexer_Token *toks, isize_t lparen, isize_t *out_rparen,
    struct Parser_ASTNode *parent, struct Sema_Scope *scope, bool add_to_scope,
    bool *out_variadic, struct Parser_Allocators *allocs, struct DiagVec *diags)
{
    struct Parser_ASTNodePVec params = {};

    isize_t rparen = Parser_find_twin_paren(toks, lparen, ISIZE_MAX);
    if (out_rparen)
        *out_rparen = rparen;

    if (rparen == -1) {
        gen_dynpush(diags, Diag_expected_token_err("')'", &toks[lparen],
                                                   ERRORTYPE_MISSING_PAREN));
        return params;
    }

    if (out_variadic)
        *out_variadic = false;

    for (isize_t i = lparen + 1; i < rparen; ++i) {
        if (toks[i].type == LEXER_TOKENTYPE_ELLIPSIS) {
            if (out_variadic)
                *out_variadic = true;
            if (i + 1 < rparen)
                gen_dynpush(diags,
                            Diag_expected_token_err("')'", &toks[lparen],
                                                    ERRORTYPE_MISSING_PAREN));
            break;
        } else {
            struct Parser_ASTNode *child;
            gen_bumpmalloc(&allocs->ast, &child);
            *child =
                (struct Parser_ASTNode){.parent = parent,
                                        .start = &toks[i],
                                        .type = PARSER_ASTNODETYPE_VAR_DECL};
            i = Parser_parse_var_decl(toks, i, PARSER_PARAM_ENDTYPES,
                                      &child->var_decl, child, scope,
                                      add_to_scope, false, allocs, diags);
            gen_dynpush(&params, child);
        }
    }

    return params;
}

static struct Sema_Scope *create_scope(struct Sema_Scope *scope,
                                       struct Parser_ASTNode *node,
                                       struct Parser_Allocators *allocs,
                                       enum Sema_ScopeType type)
{
    struct Sema_Scope *child;
    gen_bumpmalloc(&allocs->scope, &child);
    *child = (struct Sema_Scope){.parent = scope, .node = node, .type = type};
    gen_dynpush(&scope->childs, child);

    return child;
}

static void add_func_def(struct Parser_FuncDecl *decl,
                         struct Parser_ASTNode *node, struct DiagVec *diags)
{
    if (!decl->name)
        return;

    auto ident = Parser_func_ident(decl);
    if (ident->def)
        gen_dynpush(diags, Diag_ident_redefined_err(decl->name, decl->def_start,
                                                    ERRORTYPE_BAD_IDENTIFIER));
    ident->def = node;
}

static void copy_params_to_scope(struct Parser_FuncDecl *src,
                                 struct Sema_Scope *dest)
{
    for (isize_t i = 0; i < src->param_scope->idents.len; ++i) {
        auto ident = &src->param_scope->idents.arr[i];
        assert(ident->type == SEMA_IDENTTYPE_VAR);

        gen_dynpush(&dest->idents, Sema_copy_var_ident(ident));
    }
}

static struct Sema_Scope *setup_def_scope(struct Parser_FuncDecl *decl,
                                          struct Parser_ASTNode *node,
                                          struct Parser_Allocators *allocs,
                                          struct DiagVec *diags)
{
    auto def = &Parser_func_ident(decl)->func_info.def_scope;
    if (*def) {
        gen_dynpush(diags, Diag_ident_redefined_err(decl->name, node->start,
                                                    ERRORTYPE_BAD_IDENTIFIER));
    }

    *def = create_scope(Parser_func_parent(decl), node, allocs,
                        SEMA_SCOPETYPE_FUNC);
    // necessary to make the function parameters visible in the func body
    copy_params_to_scope(decl, *def);

    return *def;
}

isize_t Parser_parse_func_body(const struct Lexer_Token *toks, isize_t lcurly,
                               struct Parser_FuncDecl *decl,
                               struct Parser_ASTNode *node,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags)
{
    add_func_def(decl, node, diags);

    isize_t rcurly = Parser_find_twin_curly(toks, lcurly, ISIZE_MAX);
    if (rcurly == -1) {
        gen_dynpush(diags, Diag_expected_token_err("'}'", &toks[lcurly],
                                                   ERRORTYPE_MISSING_CURLY));
        return lcurly + 1;
    }

    auto def = setup_def_scope(decl, node, allocs, diags);

    for (isize_t i = lcurly + 1; i < rcurly;) {
        auto child =
            Parser_parse_node(toks, i, &i, node, def, false, allocs, diags);
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
            gen_dynpush(diags, Diag_expected_token_err(
                                   "]", &toks[op], ERRORTYPE_BAD_OP_OVERLOAD));
        else if (out_end)
            ++*out_end;
        return PARSER_EXPRTYPE_ARRAY_SUBSCR;

    case LEXER_TOKENTYPE_PTR_MEMB_SEL:
        return PARSER_EXPRTYPE_PTR_MEMB_SEL;

    case LEXER_TOKENTYPE_PTR_TO_PTR_MEMB_SEL:
        return PARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL;

    case LEXER_TOKENTYPE_L_PAREN:
        if (toks[op + 1].type != LEXER_TOKENTYPE_R_PAREN)
            gen_dynpush(diags, Diag_expected_token_err(
                                   ")", &toks[op], ERRORTYPE_BAD_OP_OVERLOAD));
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
                gen_dynpush(diags,
                            Diag_expected_token_err("]", &toks[op + 1],
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
                gen_dynpush(diags,
                            Diag_expected_token_err("]", &toks[op + 1],
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
                            isize_t *out_end, struct Sema_Scope *parent_scope,
                            struct Sema_Scope **out_res, struct DiagVec *diags)
{
    isize_t type_end;
    isize_t name;
    decl->type =
        Parser_parse_type(toks, start, &type_end, parent_scope, &name, diags);

    auto res = name == -1 ? parent_scope
                          : Parser_parse_scope_res(toks, name, &name,
                                                   parent_scope, diags);
    if (out_res)
        *out_res = res;

    decl->name = name == -1 || toks[name].type != LEXER_TOKENTYPE_IDENTIFIER
                     ? NULL
                     : toks[name].ident;

    if (!decl->name) {
        gen_dynpush(diags, Diag_expected_token_err("identifier", &toks[start],
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

    bool implicit_this = Parser_func_parent(decl)->type == SEMA_SCOPETYPE_CLASS;
    isize_t n_params = decl->params.len + implicit_this;
    if (n_params == 1) {
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

static void add_func_to_scope(struct Sema_Scope *scope,
                              struct Parser_FuncDecl *decl,
                              struct Parser_ASTNode *node)
{
    const struct Sema_Ident *old = Sema_add_ident(
        scope, &(struct Sema_Ident){.name = decl->name,
                                    .decl = node,
                                    .type = SEMA_IDENTTYPE_FUNC});

    if (old)
        decl->ident_idx = old - scope->idents.arr;
    else
        decl->ident_idx = scope->idents.len - 1;
}

// default_args can't be const cuz of sum discarding qualifiers in nested ptrs
// stuff idk
static bool missing_default_args(struct Parser_Expr **default_args, isize_t n,
                                 isize_t *out_bad_idx)
{
    bool found_default = false;
    for (isize_t i = 0; i < n; ++i) {
        if (default_args[i])
            found_default = true;
        else if (found_default) {
            if (out_bad_idx)
                *out_bad_idx = i;
            return true;
        }
    }

    return false;
}

static void register_default_args(struct Parser_FuncDecl *decl,
                                  struct DiagVec *diags)
{
    auto default_args = &Parser_func_ident(decl)->func_info.default_args;
    if (!*default_args)
        *default_args = mid_calloc(decl->params.len, sizeof(*default_args));

    for (isize_t i = 0; i < decl->params.len; ++i) {
        auto default_arg = (*default_args)[i];

        auto node = decl->params.arr[i];
        auto param = &node->var_decl;
        if (!param->init) // not a default arg
            continue;

        if (default_arg) {
            gen_dynpush(diags, Diag_ident_redefined_err(
                                   param->name, node->start,
                                   ERRORTYPE_BAD_DEFAULT_ARGUMENT));
            continue;
        }

        (*default_args)[i] = param->init;
    }

    isize_t bad;
    if (missing_default_args(*default_args, decl->params.len, &bad))
        gen_dynpush(diags, missing_default_arg_err(
                               decl->name, decl->params.arr[bad]->start,
                               ERRORTYPE_BAD_DEFAULT_ARGUMENT));
}

isize_t Parser_parse_func_decl(const struct Lexer_Token *toks, isize_t start,
                               struct Parser_FuncDecl *decl,
                               struct Parser_ASTNode *node,
                               struct Sema_Scope *parent_scope, bool skip_def,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags)
{
    *decl = (struct Parser_FuncDecl){.ident_idx = -1};

    struct Sema_Scope *res;
    isize_t type_end;
    parse_func_type(decl, toks, start, &type_end, parent_scope, &res, diags);
    if (toks[type_end].type != LEXER_TOKENTYPE_L_PAREN)
        CRASH("function missing left paren");

    decl->param_scope =
        create_scope(res, node, allocs, SEMA_SCOPETYPE_FUNC_PARAMS);

    isize_t lparen = type_end;
    isize_t rparen;
    decl->params =
        Parser_parse_func_params(toks, lparen, &rparen, node, decl->param_scope,
                                 true, &decl->variadic, allocs, diags);
    if (decl->is_op_overload)
        disambig_operator_overload(decl);
    if (decl->name)
        add_func_to_scope(res, decl, node);
    register_default_args(decl, diags);

    isize_t lcurly = rparen + 1;
    if (toks[lcurly].type != LEXER_TOKENTYPE_L_CURLY)
        return lcurly;
    decl->def_start = &toks[lcurly];
    decl->has_def = true;

    if (skip_def) {
        isize_t rcurly = Parser_find_twin_curly(toks, lcurly, ISIZE_MAX);
        return rcurly == -1 ? lcurly + 1 : rcurly + 1;
    } else {
        isize_t rcurly =
            Parser_parse_func_body(toks, lcurly, decl, node, allocs, diags);
        return rcurly + 1;
    }
}

bool Parser_func_is_method(const struct Parser_FuncDecl *self)
{
    return Parser_func_parent(self)->type == SEMA_SCOPETYPE_CLASS;
}
