#include "ident.h"
#include "generics/bumpalloc.h"
#include "ints.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/allocator.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "sema/scope.h"

bool Sema_is_nce_ident(enum Sema_IdentType type)
{
    return type == SEMA_IDENTTYPE_NAMESPACE || type == SEMA_IDENTTYPE_CLASS ||
           type == SEMA_IDENTTYPE_ENUM;
}

void Sema_IdentFuncInfo_deinit(struct Sema_IdentFuncInfo *self)
{
    free(self->default_args);
}

void Sema_Ident_deinit(struct Sema_Ident *self)
{
    if (self->type == SEMA_IDENTTYPE_FUNC ||
        self->type == SEMA_IDENTTYPE_TMPLT_FUNC)
        Sema_IdentFuncInfo_deinit(&self->func_info);
}

static struct Sema_IdentFuncInfo
copy_func_info(const struct Sema_IdentFuncInfo *src,
               struct Sema_Scope *dest_parent, isize_t n_params,
               bool copy_scopes, struct Parser_Allocators *allocs)
{
    struct Sema_IdentFuncInfo ret = {};

    ret.default_args = mid_malloc(n_params * sizeof(*ret.default_args));
    for (isize_t i = 0; i < n_params; ++i) {
        gen_bumpmalloc(&allocs->expr, &ret.default_args[i]);
        if (src->default_args[i])
            *ret.default_args[i] = Parser_copy_expr(src->default_args[i]);
    }

    if (copy_scopes) {
        gen_bumpmalloc(&allocs->scope, &ret.def_scope);
        Sema_copy_scope(ret.def_scope, src->def_scope, dest_parent, allocs);
    } else {
        ret.def_scope = NULL;
    }

    return ret;
}

struct Sema_Ident Sema_copy_ident(const struct Sema_Ident *src,
                                  struct Sema_Scope *dest_parent,
                                  bool copy_scopes,
                                  struct Parser_Allocators *allocs)
{
    struct Sema_Ident ret = *src;

    if (src->type == SEMA_IDENTTYPE_FUNC) {
        ret.func_info = copy_func_info(&src->func_info, dest_parent,
                                       src->decl->func_decl.params.len,
                                       copy_scopes, allocs);
    } else if (src->type == SEMA_IDENTTYPE_CLASS) {
        if (copy_scopes) {
            gen_bumpmalloc(&allocs->scope, &ret.class_info.def_scope);
            Sema_copy_scope(ret.class_info.def_scope, src->class_info.def_scope,
                            dest_parent, allocs);
        } else {
            ret.class_info.def_scope = NULL;
        }
    }

    return ret;
}

struct Sema_Scope *Sema_ident_scope(const struct Sema_Ident *self)
{
    switch (self->type) {
    case SEMA_IDENTTYPE_FUNC:
        return self->func_info.def_scope;

    case SEMA_IDENTTYPE_CLASS:
        return self->class_info.def_scope;

    case SEMA_IDENTTYPE_ENUM:
        CRASH("scope of enum ident not implemented yet");

    case SEMA_IDENTTYPE_NAMESPACE:
        return self->decl->nmspace.scope;

    default:
        CRASH("ident doesn't have it's own scope");
    }
}

bool Sema_ident_is_tmplt(enum Sema_IdentType type)
{
    return type == SEMA_IDENTTYPE_TMPLT_ALIAS ||
           type == SEMA_IDENTTYPE_TMPLT_FUNC ||
           type == SEMA_IDENTTYPE_TMPLT_CLASS;
}
