#include "parser/var_decl.h"
#include "cmd.h"
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
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/type.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct midsema_Ident *add_ident(struct midpar_VarDeclInst *inst,
                                       struct midsema_Scope *scope)
{

    return midsema_add_ident(
        scope, &(struct midsema_Ident){.name = inst->name,
                                       .decl = MIDPAR_GET_NODE(inst),
                                       .def = NULL,
                                       .type = inst->type.squals.is_typedef
                                                   ? MIDSEMA_IDENTTYPE_TYPEDEF
                                                   : MIDSEMA_IDENTTYPE_VAR});
}

void midpar_VarDeclInst_deinit(struct midpar_VarDeclInst *self)
{
    if (self->has_ctor)
        midgen_dyndeinit(&self->ctor.args, midpar_Expr_deinit);
    midpar_Type_deinit(&self->type);
}

void midpar_copy_var_decl_inst(struct midpar_VarDeclInst *dest,
                               const struct midpar_VarDeclInst *src,
                               struct midsema_Scope *dest_scope,
                               struct midpar_Allocators *allocs)
{
    *dest = *src;

    dest->type = midpar_copy_type(&src->type);

    if (src->has_ctor) {
        dest->ctor.args = (struct midpar_ExprVec){};
        midgen_dynreserve(&dest->ctor.args, src->ctor.args.len);
        for (mid_isize i = 0; i < src->ctor.args.len; ++i) {
            midgen_dynpush(&dest->ctor.args,
                           midpar_copy_expr(&src->ctor.args.arr[i]));
        }
    } else {
        if (src->init.expr) {
            midgen_bumpmalloc(&allocs->expr, &dest->init.expr);
            *dest->init.expr = midpar_copy_expr(src->init.expr);
        }
    }

    add_ident(dest, dest_scope);
}

void midpar_VarDecl_deinit(struct midpar_VarDecl *self)
{
    midgen_dyndeinit(&self->insts);
}

void midpar_copy_var_decl(struct midpar_VarDecl *dest,
                          const struct midpar_VarDecl *src,
                          struct midsema_Scope *dest_scope,
                          struct midpar_Allocators *allocs)
{
    *dest = (struct midpar_VarDecl){};

    auto inst_nodes =
        midpar_copy_nodepvec((const struct midpar_ASTNodePVec *)&src->insts,
                             MIDPAR_GET_NODE(dest), dest_scope, allocs);
    dest->insts = (struct midpar_VarDeclInstPVec){.arr = (void *)inst_nodes.arr,
                                                  .len = inst_nodes.len,
                                                  .cap = inst_nodes.cap};
}

static void resolve_auto(struct midpar_VarDeclInst *inst)
{
    assert(inst->init.expr);
    assert(inst->type.spec == MIDPAR_TYPESPEC_AUTO);
    assert(midpar_n_indir(&inst->type) == 0); // "auto *" not supported yet

    auto init_type = &inst->init.expr->ret;

    inst->type.spec = init_type->spec;
    if (init_type->spec == MIDPAR_TYPESPEC_FPTR) {
        inst->type.fptr = mid_malloc(sizeof(*inst->type.fptr));
        *inst->type.fptr = midpar_copy_fptr_type(init_type->fptr);
    } else if (init_type->spec == MIDPAR_TYPESPEC_ARRAY) {
        inst->type.array = mid_malloc(sizeof(*inst->type.array));
        *inst->type.array = midpar_copy_array_type(init_type->array);
    } else if (midpar_is_typespec_named(init_type->spec)) {
        inst->type.named = init_type->named;
    }

    // the top most CV qualifier is discarded
    for (mid_isize i = 1; i <= midpar_n_indir(init_type); ++i) {
        midgen_dynpush(&inst->type.dquals, init_type->dquals.arr[i]);
    }
}

mid_isize midpar_parse_var_decl_inst_def(
    const struct midlex_Token *toks, const enum midlex_TokenType *end_types,
    mid_isize n_end_types, struct midpar_VarDeclInst *inst,
    bool expr_prealloced, struct midsema_Scope *scope,
    struct midpar_Allocators *allocs, struct mid_DiagVec *diags)
{
    if (!inst->init.start)
        return -1;

    if (!expr_prealloced)
        midgen_bumpmalloc(&allocs->expr, &inst->init.expr);

    mid_isize start = inst->init.start - toks;
    mid_isize end;
    *inst->init.expr = midpar_parse_expr(toks, start, end_types, n_end_types,
                                         &end, scope, diags);

    if (inst->type.spec == MIDPAR_TYPESPEC_AUTO)
        resolve_auto(inst);

    return end;
}

void midpar_parse_var_decl_def(
    const struct midlex_Token *toks, const enum midlex_TokenType *end_types,
    mid_isize n_end_types, struct midpar_VarDecl *decl, bool exprs_prealloced,
    struct midsema_Scope *scope, struct midpar_Allocators *allocs,
    struct mid_DiagVec *diags)
{
    for (mid_isize i = 0; i < decl->insts.len; ++i) {
        auto inst = decl->insts.arr[i];
        midpar_parse_var_decl_inst_def(toks, end_types, n_end_types, inst,
                                       exprs_prealloced, scope, allocs, diags);
    }
}

static struct mid_Diag uninited_deduced_type_err(const char *name,
                                                 const char *type,
                                                 const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str(
            "declaration of '%s' as a deduced type '%s' needs an initializer",
            name, type),
        .err = MIDDIAG_ERR_BAD_VAR_DECLARATION,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static bool valid_name_idx(mid_isize idx, const struct midlex_Token *toks)
{
    return idx != -1 && toks[idx].type == MIDLEX_TOKENTYPE_IDENTIFIER;
}

static mid_isize parse_inst_ctor(const struct midlex_Token *toks,
                                 mid_isize lparen,
                                 struct midpar_VarDeclInst *inst,
                                 struct midsema_Scope *scope,
                                 struct mid_DiagVec *diags)
{
    mid_isize rparen = midpar_find_twin_paren(toks, lparen, MID_ISIZE_MAX);
    if (rparen == -1) {
        midgen_dynpush(diags,
                       middiag_expected_token_err("')'", &toks[lparen],
                                                  MIDDIAG_ERR_MISSING_PAREN));
        rparen = lparen;
    }

    for (mid_isize i = lparen + 1; i < rparen; ++i) {
        auto arg =
            midpar_parse_expr(toks, i, MIDPAR_ARG_ENDTYPES, &i, scope, diags);
        midgen_dynpush(&inst->ctor.args, arg);

        if (toks[i].type != MIDLEX_TOKENTYPE_R_PAREN &&
            toks[i].type != MIDLEX_TOKENTYPE_COMMA) {
            midgen_dynpush(
                diags, middiag_expected_token_err("','", &toks[lparen],
                                                  MIDDIAG_ERR_MISSING_PAREN));
        }
    }

    return rparen + 1;
}

static mid_isize
parse_inst_init(const struct midlex_Token *toks, mid_isize start,
                const enum midlex_TokenType *end_types, mid_isize n_end_types,
                struct midpar_VarDeclInst *inst, bool skip_init,
                struct midsema_Scope *scope, struct midpar_Allocators *allocs,
                struct mid_DiagVec *diags)
{
    inst->init.start = &toks[start];
    if (skip_init) {
        return midpar_skip_expr(toks, start, end_types, n_end_types, NULL);
    }
    return midpar_parse_var_decl_inst_def(toks, end_types, n_end_types, inst,
                                          false, scope, allocs, diags);
}

static struct mid_Diag void_var_err(const char *name,
                                    const struct midlex_Token *tok,
                                    enum middiag_ErrT type)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("'%s' has incomplete type 'void'", name),
        .err = type,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

mid_isize midpar_parse_var_decl_inst(
    struct midpar_VarDeclInst *self, const struct midlex_Token *toks,
    mid_isize start, const enum midlex_TokenType *end_types,
    mid_isize n_end_types, const struct midpar_Type *base,
    struct midsema_Scope *parent_scope, struct midpar_ParseVarDeclFlags flags,
    struct midpar_Allocators *allocs, struct mid_DiagVec *diags)
{
    *self = (struct midpar_VarDeclInst){};

    mid_isize type_end;
    mid_isize name;
    self->type =
        midpar_parse_type_no_base(toks, start, &type_end, base, parent_scope,
                                  &name, false, allocs, diags);

    auto res = name == -1 ? parent_scope
                          : midpar_parse_scope_res(toks, name, &name,
                                                   parent_scope, diags);
    self->name = valid_name_idx(name, toks) ? toks[name].ident : NULL;
    if (midpar_type_is_void(&self->type) && self->name)
        midgen_dynpush(diags, void_var_err(self->name, &toks[start],
                                           MIDDIAG_ERR_BAD_VAR_DECLARATION));

    if (self->name && flags.add_to_scope && add_ident(self, res))
        midgen_dynpush(diags,
                       middiag_ident_redefined_err(self->name, &toks[start],
                                                   MIDDIAG_ERR_BAD_IDENTIFIER));

    mid_isize assign_idx = type_end;
    self->has_ctor = toks[assign_idx].type == MIDLEX_TOKENTYPE_L_PAREN;
    bool has_init = toks[assign_idx].type == MIDLEX_TOKENTYPE_ASSIGN;

    mid_isize ret = type_end;
    if (self->has_ctor) {
        ret = parse_inst_ctor(toks, assign_idx, self, res, diags);
    } else if (has_init) {
        ret = parse_inst_init(toks, assign_idx + 1, end_types, n_end_types,
                              self, flags.skip_init, res, allocs, diags);
    } else if (self->type.spec == MIDPAR_TYPESPEC_AUTO) {
        midgen_dynpush(
            diags, uninited_deduced_type_err(self->name, "auto", &toks[start]));
    }

    midsema_typecheck_var_decl_inst(self, parent_scope, diags);

    return ret;
}

static bool is_end_type(const enum midlex_TokenType *end_types, mid_isize n,
                        enum midlex_TokenType type)
{
    for (mid_isize i = 0; i < n; ++i) {
        if (end_types[i] == type)
            return true;
    }

    return false;
}

mid_isize midpar_parse_var_decl_inst_list(
    const struct midlex_Token *toks, mid_isize start,
    const enum midlex_TokenType *end_types, mid_isize n_end_types,
    const struct midpar_Type *base, struct midpar_VarDeclInstPVec *insts,
    struct midpar_VarDecl *decl, struct midsema_Scope *parent_scope,
    struct midpar_ParseVarDeclFlags flags, struct midpar_Allocators *allocs,
    struct mid_DiagVec *diags)
{
    mid_isize i = start;
    do {
        struct midpar_VarDeclInst *inst;
        midgen_bumpmalloc(&allocs->ast, (void **)&inst);

        MIDPAR_GET_PARENT(inst) = MIDPAR_GET_NODE(decl);
        MIDPAR_GET_START(inst) = &toks[i];
        MIDPAR_GET_TYPE(inst) = MIDPAR_ASTNODETYPE_VAR_DECL_INST;

        i = midpar_parse_var_decl_inst(inst, toks, i, end_types, n_end_types,
                                       base, parent_scope, flags, allocs,
                                       diags);

        midgen_dynpush(insts, inst);

        if (flags.single_inst || toks[i].type != MIDLEX_TOKENTYPE_COMMA)
            break;
        ++i;
    } while (!is_end_type(end_types, n_end_types, toks[i].type));

    return i;
}

mid_isize midpar_parse_var_decl(
    struct midpar_VarDecl *self, const struct midlex_Token *toks,
    mid_isize start, const enum midlex_TokenType *end_types,
    mid_isize n_end_types, struct midpar_ParseVarDeclFlags flags,
    struct midsema_Scope *parent_scope, struct midpar_Allocators *allocs,
    struct mid_DiagVec *diags)
{
    mid_isize base_end;
    auto base =
        midpar_parse_base(toks, start, &base_end, parent_scope, allocs, diags);

    mid_isize end = midpar_parse_var_decl_inst_list(
        toks, base_end, end_types, n_end_types, &base, &self->insts, self,
        parent_scope, flags, allocs, diags);

    midpar_Type_deinit(&base);
    return end;
}

struct midpar_VarDeclInst *
midpar_decl_inst_of_name(const struct midpar_VarDecl *decl, const char *name)
{
    for (mid_isize i = 0; i < decl->insts.len; ++i) {
        if (!strcmp(decl->insts.arr[i]->name, name))
            return decl->insts.arr[i];
    }

    return NULL;
}
