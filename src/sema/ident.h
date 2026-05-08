#pragma once

#include "generics/dynarray.h"
#include "parser/expr.h"
#include <assert.h>

enum Sema_IdentType {
    SEMA_IDENTTYPE_VAR,
    SEMA_IDENTTYPE_TYPEDEF,
    SEMA_IDENTTYPE_FUNC,
    SEMA_IDENTTYPE_CLASS,
    SEMA_IDENTTYPE_ENUM,
    SEMA_IDENTTYPE_NAMESPACE,
};

struct Sema_IdentFuncInfo {
    struct Sema_Scope *def_scope;
    struct Parser_Expr **default_args; // one element for each parameter,
                                       // NULL for parameters without a default
                                       // argument
};

void Sema_IdentFuncInfo_deinit(struct Sema_IdentFuncInfo *self);

struct Sema_Ident {
    union {
        struct Sema_IdentFuncInfo func_info;
    };

    const char *name;
    struct Parser_ASTNode *decl;
    struct Parser_ASTNode *def;
    enum Sema_IdentType type;
};

gen_dynarray_struct_named(Sema_IdentVec, struct Sema_Ident);

void Sema_Ident_deinit(struct Sema_Ident *self);

// this is here in case i eventually need to make a more complex copy function
static inline struct Sema_Ident
Sema_copy_var_ident(const struct Sema_Ident *self)
{
    assert(self->type == SEMA_IDENTTYPE_VAR);
    return *self;
}
