#pragma once

#include "generics/dynarray.h"
#include "ints.h"
#include "parser/allocator.h"
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

enum midsema_IdentType {
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
bool midsema_is_nce_ident(enum midsema_IdentType type);

struct midsema_IdentFuncInfo {
    struct midsema_Scope *def_scope;
    struct midpar_Expr **default_args; // one element for each parameter,
                                       // NULL for parameters without a
                                       // default argument
};

void midsema_IdentFuncInfo_deinit(struct midsema_IdentFuncInfo *self);

struct midsema_IdentClassInfo {
    struct midsema_Scope *def_scope;
};

struct midsema_Ident {
    union {
        struct midsema_IdentFuncInfo func_info;
        struct midsema_IdentClassInfo class_info;
    };

    const char *name;
    struct midsema_Scope *parent;
    struct midpar_ASTNode *decl;
    struct midpar_ASTNode *def;
    enum midsema_IdentType type;
};
midgen_dynarray_struct_named(midsema_IdentVec, struct midsema_Ident);

void midsema_Ident_deinit(struct midsema_Ident *self);
struct midsema_Scope *midsema_ident_scope(const struct midsema_Ident *self);
bool midsema_ident_is_tmplt(enum midsema_IdentType type);

// copy_scopes     - should child scopes of the identifier be copied
struct midsema_Ident midsema_copy_ident(const struct midsema_Ident *src,
                                        struct midsema_Scope *dest_parent,
                                        bool copy_scopes,
                                        struct midpar_Allocators *allocs);

mid_isize midsema_ident_idx(const struct midsema_Ident *self);

// this is here in case i eventually need to make a more complex copy function
static inline struct midsema_Ident
midsema_copy_var_ident(const struct midsema_Ident *self)
{
    assert(self->type == MIDSEMA_IDENTTYPE_VAR);
    return *self;
}

// only gets invalidated if the identifier's idx in its scope gets invalidated
struct midsema_IdentPtr {
    struct midsema_Scope *parent; // NULL means the ptr is null
    int32_t idx;                  // -1 means the ptr is null
};
midgen_dynarray_struct_named(midsema_IdentPtrVec, struct midsema_IdentPtr);

struct midsema_IdentPtr midsema_create_identptr(struct midsema_Ident *ident);
// creates an ident ptr pointing to the last identifier in parent
struct midsema_IdentPtr midsema_identptr_to_last(struct midsema_Scope *parent);
struct midsema_IdentPtr midsema_IdentPtr_null(struct midsema_Scope *parent);
bool midsema_is_identptr_null(const struct midsema_IdentPtr *self);
struct midsema_Ident *
midsema_deref_identptr(const struct midsema_IdentPtr *self);

#ifdef __cplusplus
}
#endif
