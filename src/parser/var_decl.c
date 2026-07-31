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
#include "parser/astvec.h"
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

static struct MidSema_Ident *add_ident(struct MidParser_VarDeclInst *inst,
                                       struct MidSema_Scope *scope)
{

    return MidSema_add_ident(
        scope, &(struct MidSema_Ident){.name = inst->name,
                                       .decl = MIDPARSER_GET_NODE(inst),
                                       .def = NULL,
                                       .type = inst->type.squals.is_typedef
                                                   ? MIDSEMA_IDENTTYPE_TYPEDEF
                                                   : MIDSEMA_IDENTTYPE_VAR});
}

void MidParser_VarDeclInst_deinit(struct MidParser_VarDeclInst *self)
{
    if (self->has_ctor)
        MidGen_dyndeinit(&self->ctor.args, MidParser_Expr_deinit);
    MidParser_Type_deinit(&self->type);
}

void MidParser_copy_var_decl_inst(struct MidParser_VarDeclInst *dest,
                                  const struct MidParser_VarDeclInst *src,
                                  struct MidSema_Scope *dest_scope,
                                  struct MidParser_Allocators *allocs)
{
    *dest = *src;

    dest->type = MidParser_copy_type(&src->type);

    if (src->has_ctor) {
        dest->ctor.args = (struct MidParser_ExprVec){};
        MidGen_dynreserve(&dest->ctor.args, src->ctor.args.len);
        for (mid_isize i = 0; i < src->ctor.args.len; ++i) {
            MidGen_dynpush(&dest->ctor.args,
                        MidParser_copy_expr(&src->ctor.args.arr[i]));
        }
    } else {
        if (src->init.expr) {
            MidGen_bumpmalloc(&allocs->expr, &dest->init.expr);
            *dest->init.expr = MidParser_copy_expr(src->init.expr);
        }
    }

    add_ident(dest, dest_scope);
}

void MidParser_VarDecl_deinit(struct MidParser_VarDecl *self)
{
    MidGen_dyndeinit(&self->insts);
}

void MidParser_copy_var_decl(struct MidParser_VarDecl *dest,
                             const struct MidParser_VarDecl *src,
                             struct MidSema_Scope *dest_scope,
                             struct MidParser_Allocators *allocs)
{
    *dest = (struct MidParser_VarDecl){};

    auto inst_nodes = MidParser_copy_nodepvec(
        (const struct MidParser_ASTNodePVec *)&src->insts,
        MIDPARSER_GET_NODE(dest), dest_scope, allocs);
    dest->insts =
        (struct MidParser_VarDeclInstPVec){.arr = (void *)inst_nodes.arr,
                                           .len = inst_nodes.len,
                                           .cap = inst_nodes.cap};
}

static void resolve_auto(struct MidParser_VarDeclInst *inst)
{
    assert(inst->init.expr);
    assert(inst->type.spec == MIDPARSER_TYPESPEC_AUTO);
    assert(MidParser_n_indir(&inst->type) == 0); // "auto *" not supported yet

    auto init_type = &inst->init.expr->ret;

    inst->type.spec = init_type->spec;
    if (init_type->spec == MIDPARSER_TYPESPEC_FPTR) {
        inst->type.fptr = Mid_malloc(sizeof(*inst->type.fptr));
        *inst->type.fptr = MidParser_copy_fptr_type(init_type->fptr);
    } else if (init_type->spec == MIDPARSER_TYPESPEC_ARRAY) {
        inst->type.array = Mid_malloc(sizeof(*inst->type.array));
        *inst->type.array = MidParser_copy_array_type(init_type->array);
    } else if (MidParser_is_typespec_named(init_type->spec)) {
        inst->type.named = init_type->named;
    }

    // the top most CV qualifier is discarded
    for (mid_isize i = 1; i <= MidParser_n_indir(init_type); ++i) {
        MidGen_dynpush(&inst->type.dquals, init_type->dquals.arr[i]);
    }
}

mid_isize MidParser_parse_var_decl_inst_def(
    const struct MidLexer_Token *toks, const enum MidLexer_TokenType *end_types,
    mid_isize n_end_types, struct MidParser_VarDeclInst *inst,
    bool expr_prealloced, struct MidSema_Scope *scope,
    struct MidParser_Allocators *allocs, struct MidDiag_DiagVec *diags)
{
    if (!inst->init.start)
        return -1;

    if (!expr_prealloced)
        MidGen_bumpmalloc(&allocs->expr, &inst->init.expr);

    mid_isize start = inst->init.start - toks;
    mid_isize end;
    *inst->init.expr = MidParser_parse_expr(toks, start, end_types, n_end_types,
                                            &end, scope, diags);

    if (inst->type.spec == MIDPARSER_TYPESPEC_AUTO)
        resolve_auto(inst);

    return end;
}

void MidParser_parse_var_decl_def(
    const struct MidLexer_Token *toks, const enum MidLexer_TokenType *end_types,
    mid_isize n_end_types, struct MidParser_VarDecl *decl, bool exprs_prealloced,
    struct MidSema_Scope *scope, struct MidParser_Allocators *allocs,
    struct MidDiag_DiagVec *diags)
{
    for (mid_isize i = 0; i < decl->insts.len; ++i) {
        auto inst = decl->insts.arr[i];
        MidParser_parse_var_decl_inst_def(toks, end_types, n_end_types, inst,
                                          exprs_prealloced, scope, allocs,
                                          diags);
    }
}

static struct MidDiag_Diag
uninited_deduced_type_err(const char *name, const char *type,
                          const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str(
            "declaration of '%s' as a deduced type '%s' needs an initializer",
            name, type),
        .err = MIDDIAG_ERR_BAD_VAR_DECLARATION,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static bool valid_name_idx(mid_isize idx, const struct MidLexer_Token *toks)
{
    return idx != -1 && toks[idx].type == MIDLEXER_TOKENTYPE_IDENTIFIER;
}

static mid_isize parse_inst_ctor(const struct MidLexer_Token *toks,
                               mid_isize lparen,
                               struct MidParser_VarDeclInst *inst,
                               struct MidSema_Scope *scope,
                               struct MidDiag_DiagVec *diags)
{
    mid_isize rparen = MidParser_find_twin_paren(toks, lparen, MID_ISIZE_MAX);
    if (rparen == -1) {
        MidGen_dynpush(diags,
                    MidDiag_expected_token_err("')'", &toks[lparen],
                                               MIDDIAG_ERR_MISSING_PAREN));
        rparen = lparen;
    }

    for (mid_isize i = lparen + 1; i < rparen; ++i) {
        auto arg = MidParser_parse_expr(toks, i, MIDPARSER_ARG_ENDTYPES, &i,
                                        scope, diags);
        MidGen_dynpush(&inst->ctor.args, arg);

        if (toks[i].type != MIDLEXER_TOKENTYPE_R_PAREN &&
            toks[i].type != MIDLEXER_TOKENTYPE_COMMA) {
            MidGen_dynpush(diags,
                        MidDiag_expected_token_err("','", &toks[lparen],
                                                   MIDDIAG_ERR_MISSING_PAREN));
        }
    }

    return rparen + 1;
}

static mid_isize parse_inst_init(const struct MidLexer_Token *toks, mid_isize start,
                               const enum MidLexer_TokenType *end_types,
                               mid_isize n_end_types,
                               struct MidParser_VarDeclInst *inst,
                               bool skip_init, struct MidSema_Scope *scope,
                               struct MidParser_Allocators *allocs,
                               struct MidDiag_DiagVec *diags)
{
    inst->init.start = &toks[start];
    if (skip_init) {
        return MidParser_skip_expr(toks, start, end_types, n_end_types, NULL);
    }
    return MidParser_parse_var_decl_inst_def(toks, end_types, n_end_types, inst,
                                             false, scope, allocs, diags);
}

static struct MidDiag_Diag void_var_err(const char *name,
                                        const struct MidLexer_Token *tok,
                                        enum MidDiag_ErrT type)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("'%s' has incomplete type 'void'", name),
        .err = type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

mid_isize MidParser_parse_var_decl_inst(
    struct MidParser_VarDeclInst *self, const struct MidLexer_Token *toks,
    mid_isize start, const enum MidLexer_TokenType *end_types,
    mid_isize n_end_types, const struct MidParser_Type *base,
    struct MidSema_Scope *parent_scope,
    struct MidParser_ParseVarDeclFlags flags,
    struct MidParser_Allocators *allocs, struct MidDiag_DiagVec *diags)
{
    *self = (struct MidParser_VarDeclInst){};

    mid_isize type_end;
    mid_isize name;
    self->type =
        MidParser_parse_type_no_base(toks, start, &type_end, base, parent_scope,
                                     &name, false, allocs, diags);

    auto res = name == -1 ? parent_scope
                          : MidParser_parse_scope_res(toks, name, &name,
                                                      parent_scope, diags);
    self->name = valid_name_idx(name, toks) ? toks[name].ident : NULL;
    if (MidParser_type_is_void(&self->type) && self->name)
        MidGen_dynpush(diags, void_var_err(self->name, &toks[start],
                                        MIDDIAG_ERR_BAD_VAR_DECLARATION));

    if (self->name && flags.add_to_scope && add_ident(self, res))
        MidGen_dynpush(diags,
                    MidDiag_ident_redefined_err(self->name, &toks[start],
                                                MIDDIAG_ERR_BAD_IDENTIFIER));

    mid_isize assign_idx = type_end;
    self->has_ctor = toks[assign_idx].type == MIDLEXER_TOKENTYPE_L_PAREN;
    bool has_init = toks[assign_idx].type == MIDLEXER_TOKENTYPE_ASSIGN;

    mid_isize ret = type_end;
    if (self->has_ctor) {
        ret = parse_inst_ctor(toks, assign_idx, self, res, diags);
    } else if (has_init) {
        ret = parse_inst_init(toks, assign_idx + 1, end_types, n_end_types,
                              self, flags.skip_init, res, allocs, diags);
    } else if (self->type.spec == MIDPARSER_TYPESPEC_AUTO) {
        MidGen_dynpush(
            diags, uninited_deduced_type_err(self->name, "auto", &toks[start]));
    }

    MidSema_typecheck_var_decl_inst(self, diags);

    return ret;
}

static bool is_end_type(const enum MidLexer_TokenType *end_types, mid_isize n,
                        enum MidLexer_TokenType type)
{
    for (mid_isize i = 0; i < n; ++i) {
        if (end_types[i] == type)
            return true;
    }

    return false;
}

mid_isize MidParser_parse_var_decl_inst_list(
    const struct MidLexer_Token *toks, mid_isize start,
    const enum MidLexer_TokenType *end_types, mid_isize n_end_types,
    const struct MidParser_Type *base, struct MidParser_VarDeclInstPVec *insts,
    struct MidParser_VarDecl *decl, struct MidSema_Scope *parent_scope,
    struct MidParser_ParseVarDeclFlags flags,
    struct MidParser_Allocators *allocs, struct MidDiag_DiagVec *diags)
{
    mid_isize i = start;
    do {
        struct MidParser_VarDeclInst *inst;
        MidGen_bumpmalloc(&allocs->ast, (void **)&inst);

        MIDPARSER_GET_PARENT(inst) = MIDPARSER_GET_NODE(decl);
        MIDPARSER_GET_START(inst) = &toks[i];
        MIDPARSER_GET_TYPE(inst) = MIDPARSER_ASTNODETYPE_VAR_DECL_INST;

        i = MidParser_parse_var_decl_inst(inst, toks, i, end_types, n_end_types,
                                          base, parent_scope, flags, allocs,
                                          diags);

        MidGen_dynpush(insts, inst);

        if (flags.single_inst || toks[i].type != MIDLEXER_TOKENTYPE_COMMA)
            break;
        ++i;
    } while (!is_end_type(end_types, n_end_types, toks[i].type));

    return i;
}

mid_isize MidParser_parse_var_decl(
    struct MidParser_VarDecl *self, const struct MidLexer_Token *toks,
    mid_isize start, const enum MidLexer_TokenType *end_types,
    mid_isize n_end_types, struct MidParser_ParseVarDeclFlags flags,
    struct MidSema_Scope *parent_scope, struct MidParser_Allocators *allocs,
    struct MidDiag_DiagVec *diags)
{
    mid_isize base_end;
    auto base = MidParser_parse_base(toks, start, &base_end, parent_scope,
                                     allocs, diags);

    mid_isize end = MidParser_parse_var_decl_inst_list(
        toks, base_end, end_types, n_end_types, &base, &self->insts, self,
        parent_scope, flags, allocs, diags);

    MidParser_Type_deinit(&base);
    return end;
}

struct MidParser_VarDeclInst *
MidParser_decl_inst_of_name(const struct MidParser_VarDecl *decl,
                            const char *name)
{
    for (mid_isize i = 0; i < decl->insts.len; ++i) {
        if (!strcmp(decl->insts.arr[i]->name, name))
            return decl->insts.arr[i];
    }

    return NULL;
}
