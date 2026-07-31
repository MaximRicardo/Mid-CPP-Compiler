#include "ident.h"
#include "generics/bumpalloc.h"
#include "ints.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "sema/scope.h"

bool MidSema_is_nce_ident(enum MidSema_IdentType type)
{
    return type == MIDSEMA_IDENTTYPE_NAMESPACE || type == MIDSEMA_IDENTTYPE_CLASS ||
           type == MIDSEMA_IDENTTYPE_ENUM;
}

void MidSema_IdentFuncInfo_deinit(struct MidSema_IdentFuncInfo *self)
{
    free(self->default_args);
}

void MidSema_Ident_deinit(struct MidSema_Ident *self)
{
    if (self->type == MIDSEMA_IDENTTYPE_FUNC ||
        self->type == MIDSEMA_IDENTTYPE_TMPLT_FUNC)
        MidSema_IdentFuncInfo_deinit(&self->func_info);
}

static struct MidSema_IdentFuncInfo
copy_func_info(const struct MidSema_IdentFuncInfo *src,
               struct MidSema_Scope *dest_parent, mid_isize n_params,
               bool copy_scopes, struct MidParser_Allocators *allocs)
{
    struct MidSema_IdentFuncInfo ret = {};

    ret.default_args = Mid_malloc(n_params * sizeof(*ret.default_args));
    for (mid_isize i = 0; i < n_params; ++i) {
        MidGen_bumpmalloc(&allocs->expr, &ret.default_args[i]);
        if (src->default_args[i])
            *ret.default_args[i] = MidParser_copy_expr(src->default_args[i]);
    }

    if (copy_scopes) {
        MidGen_bumpmalloc(&allocs->scope, &ret.def_scope);
        MidSema_copy_scope(ret.def_scope, src->def_scope, dest_parent, allocs);
    } else {
        ret.def_scope = NULL;
    }

    return ret;
}

struct MidSema_Ident MidSema_copy_ident(const struct MidSema_Ident *src,
                                  struct MidSema_Scope *dest_parent,
                                  bool copy_scopes,
                                  struct MidParser_Allocators *allocs)
{
    struct MidSema_Ident ret = *src;
    ret.parent = dest_parent;

    if (src->type == MIDSEMA_IDENTTYPE_FUNC) {
        ret.func_info = copy_func_info(&src->func_info, ret.parent,
                                       src->decl->func_decl.params.len,
                                       copy_scopes, allocs);
    } else if (src->type == MIDSEMA_IDENTTYPE_CLASS) {
        if (copy_scopes) {
            MidGen_bumpmalloc(&allocs->scope, &ret.class_info.def_scope);
            MidSema_copy_scope(ret.class_info.def_scope, src->class_info.def_scope,
                            ret.parent, allocs);
        } else {
            ret.class_info.def_scope = NULL;
        }
    }

    return ret;
}

struct MidSema_Scope *MidSema_ident_scope(const struct MidSema_Ident *self)
{
    switch (self->type) {
    case MIDSEMA_IDENTTYPE_FUNC:
        return self->func_info.def_scope;

    case MIDSEMA_IDENTTYPE_CLASS:
        return self->class_info.def_scope;

    case MIDSEMA_IDENTTYPE_ENUM:
        MID_CRASH("scope of enum ident not implemented yet");

    case MIDSEMA_IDENTTYPE_NAMESPACE:
        return self->decl->nmspace.scope;

    default:
        MID_CRASH("ident doesn't have it's own scope");
    }
}

bool MidSema_ident_is_tmplt(enum MidSema_IdentType type)
{
    return type == MIDSEMA_IDENTTYPE_TMPLT_ALIAS ||
           type == MIDSEMA_IDENTTYPE_TMPLT_FUNC ||
           type == MIDSEMA_IDENTTYPE_TMPLT_CLASS;
}

mid_isize MidSema_ident_idx(const struct MidSema_Ident *self)
{
    return self - self->parent->idents.arr;
}

struct MidSema_IdentPtr MidSema_create_identptr(struct MidSema_Ident *ident)
{
    return (struct MidSema_IdentPtr){.parent = ident->parent,
                                  .idx = MidSema_ident_idx(ident)};
}

struct MidSema_IdentPtr MidSema_identptr_to_last(struct MidSema_Scope *parent)
{
    return (struct MidSema_IdentPtr){.parent = parent,
                                  .idx = parent->idents.len - 1};
}

struct MidSema_IdentPtr MidSema_IdentPtr_null(struct MidSema_Scope *parent)
{
    return (struct MidSema_IdentPtr){.parent = parent, .idx = -1};
}

bool MidSema_is_identptr_null(const struct MidSema_IdentPtr *self)
{
    return !self->parent || self->idx == -1;
}

struct MidSema_Ident *MidSema_deref_identptr(const struct MidSema_IdentPtr *self)
{
    return &self->parent->idents.arr[self->idx];
}
