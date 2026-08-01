#include "ident.h"
#include "generics/bumpalloc.h"
#include "ints.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "sema/scope.h"

bool midsema_is_nce_ident(enum midsema_IdentType type)
{
    return type == MIDSEMA_IDENTTYPE_NAMESPACE ||
           type == MIDSEMA_IDENTTYPE_CLASS || type == MIDSEMA_IDENTTYPE_ENUM;
}

void midsema_IdentFuncInfo_deinit(struct midsema_IdentFuncInfo *self)
{
    free(self->default_args);
}

void midsema_Ident_deinit(struct midsema_Ident *self)
{
    if (self->type == MIDSEMA_IDENTTYPE_FUNC ||
        self->type == MIDSEMA_IDENTTYPE_TMPLT_FUNC)
        midsema_IdentFuncInfo_deinit(&self->func_info);
}

static struct midsema_IdentFuncInfo
copy_func_info(const struct midsema_IdentFuncInfo *src,
               struct midsema_Scope *dest_parent, mid_isize n_params,
               bool copy_scopes, struct midpar_Allocators *allocs)
{
    struct midsema_IdentFuncInfo ret = {};

    ret.default_args = mid_malloc(n_params * sizeof(*ret.default_args));
    for (mid_isize i = 0; i < n_params; ++i) {
        midgen_bumpmalloc(&allocs->expr, &ret.default_args[i]);
        if (src->default_args[i])
            *ret.default_args[i] = midpar_copy_expr(src->default_args[i]);
    }

    if (copy_scopes) {
        midgen_bumpmalloc(&allocs->scope, &ret.def_scope);
        midsema_copy_scope(ret.def_scope, src->def_scope, dest_parent, allocs);
    } else {
        ret.def_scope = NULL;
    }

    return ret;
}

struct midsema_Ident midsema_copy_ident(const struct midsema_Ident *src,
                                        struct midsema_Scope *dest_parent,
                                        bool copy_scopes,
                                        struct midpar_Allocators *allocs)
{
    struct midsema_Ident ret = *src;
    ret.parent = dest_parent;

    if (src->type == MIDSEMA_IDENTTYPE_FUNC) {
        ret.func_info = copy_func_info(&src->func_info, ret.parent,
                                       src->decl->func_decl.params.len,
                                       copy_scopes, allocs);
    } else if (src->type == MIDSEMA_IDENTTYPE_CLASS) {
        if (copy_scopes) {
            midgen_bumpmalloc(&allocs->scope, &ret.class_info.def_scope);
            midsema_copy_scope(ret.class_info.def_scope,
                               src->class_info.def_scope, ret.parent, allocs);
        } else {
            ret.class_info.def_scope = NULL;
        }
    }

    return ret;
}

struct midsema_Scope *midsema_ident_scope(const struct midsema_Ident *self)
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

bool midsema_ident_is_tmplt(enum midsema_IdentType type)
{
    return type == MIDSEMA_IDENTTYPE_TMPLT_ALIAS ||
           type == MIDSEMA_IDENTTYPE_TMPLT_FUNC ||
           type == MIDSEMA_IDENTTYPE_TMPLT_CLASS;
}

mid_isize midsema_ident_idx(const struct midsema_Ident *self)
{
    return self - self->parent->idents.arr;
}

struct midsema_IdentPtr midsema_create_identptr(struct midsema_Ident *ident)
{
    return (struct midsema_IdentPtr){.parent = ident->parent,
                                     .idx = midsema_ident_idx(ident)};
}

struct midsema_IdentPtr midsema_identptr_to_last(struct midsema_Scope *parent)
{
    return (struct midsema_IdentPtr){.parent = parent,
                                     .idx = parent->idents.len - 1};
}

struct midsema_IdentPtr midsema_IdentPtr_null(struct midsema_Scope *parent)
{
    return (struct midsema_IdentPtr){.parent = parent, .idx = -1};
}

bool midsema_is_identptr_null(const struct midsema_IdentPtr *self)
{
    return !self->parent || self->idx == -1;
}

struct midsema_Ident *
midsema_deref_identptr(const struct midsema_IdentPtr *self)
{
    return &self->parent->idents.arr[self->idx];
}
