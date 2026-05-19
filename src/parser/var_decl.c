#include "var_decl.h"
#include "diag.h"
#include "generics/bumpalloc.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "mid_alloc.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/end_types.h"
#include "parser/expr.h"
#include "parser/find_twin.h"
#include "parser/scope.h"
#include "parser/type.h"
#include "print.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/type.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Parser_VarDeclInst_deinit(struct Parser_VarDeclInst *self)
{
    if (self->has_ctor)
        gen_dyndeinit(&self->ctor.args, Parser_Expr_deinit);
    Parser_Type_deinit(&self->type);
}

void Parser_VarDecl_deinit(struct Parser_VarDecl *self)
{
    gen_dyndeinit(&self->insts, Parser_VarDeclInst_deinit);
}

static struct Sema_Ident *add_ident(const struct Parser_VarDeclInst *inst,
                                    struct Parser_ASTNode *node,
                                    struct Sema_Scope *scope)
{

    return Sema_add_ident(
        scope, &(struct Sema_Ident){.name = inst->name,
                                    .decl = node,
                                    .def = NULL,
                                    .type = inst->type.squals.is_typedef
                                                ? SEMA_IDENTTYPE_TYPEDEF
                                                : SEMA_IDENTTYPE_VAR});
}

static void resolve_auto(struct Parser_VarDeclInst *inst)
{
    assert(inst->init.expr);
    assert(inst->type.spec == PARSER_TYPESPEC_AUTO);
    assert(Parser_n_indir(&inst->type) == 0); // "auto *" not supported yet

    auto init_type = &inst->init.expr->ret;

    inst->type.spec = init_type->spec;
    if (init_type->spec == PARSER_TYPESPEC_FPTR) {
        inst->type.fptr = mid_malloc(sizeof(*inst->type.fptr));
        *inst->type.fptr = Parser_copy_fptr_type(init_type->fptr);
    } else if (init_type->spec == PARSER_TYPESPEC_ARRAY) {
        inst->type.array = mid_malloc(sizeof(*inst->type.array));
        *inst->type.array = Parser_copy_array_type(init_type->array);
    } else if (Parser_is_typespec_named(init_type->spec)) {
        inst->type.named = init_type->named;
    }

    // the top most CV qualifier is discarded
    for (isize_t i = 1; i <= Parser_n_indir(init_type); ++i) {
        gen_dynpush(&inst->type.dquals, init_type->dquals.arr[i]);
    }
}

isize_t Parser_parse_var_decl_inst_def(
    const struct Lexer_Token *toks, const enum Lexer_TokenType *end_types,
    isize_t n_end_types, struct Parser_VarDeclInst *inst, bool expr_prealloced,
    struct Sema_Scope *scope, struct Parser_Allocators *allocs,
    struct DiagVec *diags)
{
    if (!inst->init.start)
        return -1;

    if (!expr_prealloced)
        gen_bumpmalloc(&allocs->expr, &inst->init.expr);

    isize_t start = inst->init.start - toks;
    isize_t end;
    *inst->init.expr = Parser_parse_expr(toks, start, end_types, n_end_types,
                                         &end, scope, diags);

    if (inst->type.spec == PARSER_TYPESPEC_AUTO)
        resolve_auto(inst);

    return end;
}

void Parser_parse_var_decl_def(const struct Lexer_Token *toks,
                               const enum Lexer_TokenType *end_types,
                               isize_t n_end_types, struct Parser_VarDecl *decl,
                               bool exprs_prealloced, struct Sema_Scope *scope,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags)
{
    for (isize_t i = 0; i < decl->insts.len; ++i) {
        auto inst = &decl->insts.arr[i];
        Parser_parse_var_decl_inst_def(toks, end_types, n_end_types, inst,
                                       exprs_prealloced, scope, allocs, diags);
    }
}

static struct Diag uninited_deduced_type_err(const char *name, const char *type,
                                             const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str(
            "declaration of '%s' as a deduced type '%s' needs an initializer",
            name, type),
        .err = ERRORTYPE_BAD_VAR_DECLARATION,
        .is_err = true,
    };
}

static bool valid_name_idx(isize_t idx, const struct Lexer_Token *toks)
{
    return idx != -1 && toks[idx].type == LEXER_TOKENTYPE_IDENTIFIER;
}

static isize_t parse_inst_ctor(const struct Lexer_Token *toks, isize_t lparen,
                               struct Parser_VarDeclInst *inst,
                               struct Sema_Scope *scope, struct DiagVec *diags)
{
    isize_t rparen = Parser_find_twin_paren(toks, lparen, ISIZE_MAX);
    if (rparen == -1) {
        gen_dynpush(diags, Diag_expected_token_err("')'", &toks[lparen],
                                                   ERRORTYPE_MISSING_PAREN));
        rparen = lparen;
    }

    for (isize_t i = lparen + 1; i < rparen; ++i) {
        auto arg =
            Parser_parse_expr(toks, i, PARSER_ARG_ENDTYPES, &i, scope, diags);
        gen_dynpush(&inst->ctor.args, arg);

        if (toks[i].type != LEXER_TOKENTYPE_R_PAREN &&
            toks[i].type != LEXER_TOKENTYPE_COMMA) {
            gen_dynpush(diags,
                        Diag_expected_token_err("','", &toks[lparen],
                                                ERRORTYPE_MISSING_PAREN));
        }
    }

    return rparen + 1;
}

static isize_t parse_inst_init(const struct Lexer_Token *toks, isize_t start,
                               const enum Lexer_TokenType *end_types,
                               isize_t n_end_types,
                               struct Parser_VarDeclInst *inst, bool skip_init,
                               struct Sema_Scope *scope,
                               struct Parser_Allocators *allocs,
                               struct DiagVec *diags)
{
    inst->init.start = &toks[start];
    if (skip_init) {
        return Parser_skip_expr(toks, start, end_types, n_end_types, NULL);
    }
    return Parser_parse_var_decl_inst_def(toks, end_types, n_end_types, inst,
                                          false, scope, allocs, diags);
}

isize_t Parser_parse_var_decl_inst(
    const struct Lexer_Token *toks, isize_t start,
    const enum Lexer_TokenType *end_types, isize_t n_end_types,
    const struct Parser_Type *base, struct Parser_VarDeclInst *inst,
    struct Sema_Scope *parent_scope, struct Parser_ASTNode *node,
    struct Parser_ParseVarDeclFlags flags, struct Parser_Allocators *allocs,
    struct DiagVec *diags)
{
    *inst = (struct Parser_VarDeclInst){.start = &toks[start]};

    isize_t type_end;
    isize_t name;
    inst->type = Parser_parse_type_no_base(toks, start, &type_end, base,
                                           parent_scope, &name, diags);

    auto res = name == -1 ? parent_scope
                          : Parser_parse_scope_res(toks, name, &name,
                                                   parent_scope, diags);
    inst->name = valid_name_idx(name, toks) ? toks[name].ident : NULL;

    if (inst->name && flags.add_to_scope && add_ident(inst, node, res))
        gen_dynpush(diags, Diag_ident_redefined_err(inst->name, &toks[start],
                                                    ERRORTYPE_BAD_IDENTIFIER));

    isize_t assign_idx = type_end;
    inst->has_ctor = toks[assign_idx].type == LEXER_TOKENTYPE_L_PAREN;
    bool has_init = toks[assign_idx].type == LEXER_TOKENTYPE_ASSIGN;

    isize_t ret = type_end;
    if (inst->has_ctor) {
        ret = parse_inst_ctor(toks, assign_idx, inst, res, diags);
    } else if (has_init) {
        ret = parse_inst_init(toks, assign_idx + 1, end_types, n_end_types,
                              inst, flags.skip_init, res, allocs, diags);
    } else if (inst->type.spec == PARSER_TYPESPEC_AUTO) {
        gen_dynpush(
            diags, uninited_deduced_type_err(inst->name, "auto", &toks[start]));
    }

    Sema_typecheck_var_decl_inst(inst, diags);

    return ret;
}

static bool is_end_type(const enum Lexer_TokenType *end_types, isize_t n,
                        enum Lexer_TokenType type)
{
    for (isize_t i = 0; i < n; ++i) {
        if (end_types[i] == type)
            return true;
    }

    return false;
}

isize_t Parser_parse_var_decl_inst_list(
    const struct Lexer_Token *toks, isize_t start,
    const enum Lexer_TokenType *end_types, isize_t n_end_types,
    const struct Parser_Type *base, struct Parser_VarDeclInstVec *insts,
    struct Parser_ASTNode *node, struct Sema_Scope *parent_scope,
    struct Parser_ParseVarDeclFlags flags, struct Parser_Allocators *allocs,
    struct DiagVec *diags)
{
    isize_t i = start;
    do {
        gen_dynpush(insts, (struct Parser_VarDeclInst){});
        auto inst = &insts->arr[insts->len - 1];

        i = Parser_parse_var_decl_inst(toks, i, end_types, n_end_types, base,
                                       inst, parent_scope, node, flags, allocs,
                                       diags);

        if (flags.single_inst || toks[i].type != LEXER_TOKENTYPE_COMMA)
            break;
        ++i;
    } while (!is_end_type(end_types, n_end_types, toks[i].type));

    return i;
}

isize_t Parser_parse_var_decl(const struct Lexer_Token *toks, isize_t start,
                              const enum Lexer_TokenType *end_types,
                              isize_t n_end_types, struct Parser_ASTNode *node,
                              struct Parser_ParseVarDeclFlags flags,
                              struct Sema_Scope *parent_scope,
                              struct Parser_Allocators *allocs,
                              struct DiagVec *diags)
{
    auto decl = &node->var_decl;
    *decl = (struct Parser_VarDecl){};

    isize_t base_end;
    auto base = Parser_parse_base(toks, start, &base_end, parent_scope, diags);

    isize_t end = Parser_parse_var_decl_inst_list(
        toks, base_end, end_types, n_end_types, &base, &decl->insts, node,
        parent_scope, flags, allocs, diags);

    Parser_Type_deinit(&base);
    return end;
}

const struct Parser_VarDeclInst *
Parser_decl_inst_of_name_const(const struct Parser_VarDecl *decl,
                               const char *name)
{
    for (isize_t i = 0; i < decl->insts.len; ++i) {
        if (!strcmp(decl->insts.arr[i].name, name))
            return &decl->insts.arr[i];
    }

    return NULL;
}

struct Parser_VarDeclInst *Parser_decl_inst_of_name(struct Parser_VarDecl *decl,
                                                    const char *name)
{
    return (struct Parser_VarDeclInst *)Parser_decl_inst_of_name_const(decl,
                                                                       name);
}
