#pragma once

#include "generics/dynarray.h"
#include "ints.h"
#include "parser/allocator.h"
#include <assert.h>

enum MidSema_IdentType {
    MIDSEMA_IDENTTYPE_VAR,
    MIDSEMA_IDENTTYPE_TYPEDEF, // TODO: rename this to alias instead of typedef
    MIDSEMA_IDENTTYPE_FUNC,
    MIDSEMA_IDENTTYPE_CLASS,
    MIDSEMA_IDENTTYPE_ENUM,
    MIDSEMA_IDENTTYPE_NAMESPACE,
    MIDSEMA_IDENTTYPE_TMPLT_CLASS,
    MIDSEMA_IDENTTYPE_TMPLT_FUNC,
    MIDSEMA_IDENTTYPE_TMPLT_ALIAS,
};

// NCE - Namespace, Class, or Enum
bool MidSema_is_nce_ident(enum MidSema_IdentType type);

struct MidSema_IdentFuncInfo {
    struct MidSema_Scope *def_scope;
    struct MidParser_Expr **default_args; // one element for each parameter,
                                          // NULL for parameters without a
                                          // default argument
};

void MidSema_IdentFuncInfo_deinit(struct MidSema_IdentFuncInfo *self);

struct MidSema_IdentClassInfo {
    struct MidSema_Scope *def_scope;
};

struct MidSema_Ident {
    union {
        struct MidSema_IdentFuncInfo func_info;
        struct MidSema_IdentClassInfo class_info;
    };

    const char *name;
    struct MidSema_Scope *parent;
    struct MidParser_ASTNode *decl;
    struct MidParser_ASTNode *def;
    enum MidSema_IdentType type;
};
MidGen_dynarray_struct_named(MidSema_IdentVec, struct MidSema_Ident);

void MidSema_Ident_deinit(struct MidSema_Ident *self);
struct MidSema_Scope *MidSema_ident_scope(const struct MidSema_Ident *self);
bool MidSema_ident_is_tmplt(enum MidSema_IdentType type);

// copy_scopes     - should child scopes of the identifier be copied
struct MidSema_Ident MidSema_copy_ident(const struct MidSema_Ident *src,
                                        struct MidSema_Scope *dest_parent,
                                        bool copy_scopes,
                                        struct MidParser_Allocators *allocs);

mid_isize MidSema_ident_idx(const struct MidSema_Ident *self);

// this is here in case i eventually need to make a more complex copy function
static inline struct MidSema_Ident
MidSema_copy_var_ident(const struct MidSema_Ident *self)
{
    assert(self->type == MIDSEMA_IDENTTYPE_VAR);
    return *self;
}

// only gets invalidated if the identifier's idx in its scope gets invalidated
struct MidSema_IdentPtr {
    struct MidSema_Scope *parent; // NULL means the ptr is null
    i32 idx;                      // -1 means the ptr is null
};
MidGen_dynarray_struct_named(MidSema_IdentPtrVec, struct MidSema_IdentPtr);

struct MidSema_IdentPtr MidSema_create_identptr(struct MidSema_Ident *ident);
// creates an ident ptr pointing to the last identifier in parent
struct MidSema_IdentPtr MidSema_identptr_to_last(struct MidSema_Scope *parent);
struct MidSema_IdentPtr MidSema_IdentPtr_null(struct MidSema_Scope *parent);
bool MidSema_is_identptr_null(const struct MidSema_IdentPtr *self);
struct MidSema_Ident *
MidSema_deref_identptr(const struct MidSema_IdentPtr *self);
