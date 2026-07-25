#pragma once

#include "generics/dynarray.h"
#include "ints.h"
#include "parser/allocator.h"
#include <assert.h>

enum Sema_IdentType {
    SEMA_IDENTTYPE_VAR,
    SEMA_IDENTTYPE_TYPEDEF, // TODO: rename this to alias instead of typedef
    SEMA_IDENTTYPE_FUNC,
    SEMA_IDENTTYPE_CLASS,
    SEMA_IDENTTYPE_ENUM,
    SEMA_IDENTTYPE_NAMESPACE,
    SEMA_IDENTTYPE_TMPLT_CLASS,
    SEMA_IDENTTYPE_TMPLT_FUNC,
    SEMA_IDENTTYPE_TMPLT_ALIAS,
};

// NCE - Namespace, Class, or Enum
bool Sema_is_nce_ident(enum Sema_IdentType type);

struct Sema_IdentFuncInfo {
    struct Sema_Scope *def_scope;
    struct Parser_Expr **default_args; // one element for each parameter,
                                       // NULL for parameters without a default
                                       // argument
};

void Sema_IdentFuncInfo_deinit(struct Sema_IdentFuncInfo *self);

struct Sema_IdentClassInfo {
    struct Sema_Scope *def_scope;
};

struct Sema_Ident {
    union {
        struct Sema_IdentFuncInfo func_info;
        struct Sema_IdentClassInfo class_info;
    };

    const char *name;
    struct Sema_Scope *parent;
    struct Parser_ASTNode *decl;
    struct Parser_ASTNode *def;
    enum Sema_IdentType type;
};
gen_dynarray_struct_named(Sema_IdentVec, struct Sema_Ident);

void Sema_Ident_deinit(struct Sema_Ident *self);
struct Sema_Scope *Sema_ident_scope(const struct Sema_Ident *self);
bool Sema_ident_is_tmplt(enum Sema_IdentType type);

// copy_scopes     - should child scopes of the identifier be copied
struct Sema_Ident Sema_copy_ident(const struct Sema_Ident *src,
                                  struct Sema_Scope *dest_parent,
                                  bool copy_scopes,
                                  struct Parser_Allocators *allocs);

isize_t Sema_ident_idx(const struct Sema_Ident *self);

// this is here in case i eventually need to make a more complex copy function
static inline struct Sema_Ident
Sema_copy_var_ident(const struct Sema_Ident *self)
{
    assert(self->type == SEMA_IDENTTYPE_VAR);
    return *self;
}

// only gets invalidated if the identifier's idx in its scope gets invalidated
struct Sema_IdentPtr {
    struct Sema_Scope *parent; // NULL means the ptr is null
    i32 idx;                   // -1 means the ptr is null
};
gen_dynarray_struct_named(Sema_IdentPtrVec, struct Sema_IdentPtr);

struct Sema_IdentPtr Sema_create_identptr(struct Sema_Ident *ident);
// creates an ident ptr pointing to the last identifier in parent
struct Sema_IdentPtr Sema_identptr_to_last(struct Sema_Scope *parent);
struct Sema_IdentPtr Sema_IdentPtr_null(struct Sema_Scope *parent);
bool Sema_is_identptr_null(const struct Sema_IdentPtr *self);
struct Sema_Ident *Sema_deref_identptr(const struct Sema_IdentPtr *self);
