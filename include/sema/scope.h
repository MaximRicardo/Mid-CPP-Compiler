#pragma once

#include "ident.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/type.h"

#ifdef __cplusplus
extern "C" {
#endif

struct midpar_ASTNode;

enum midsema_ScopeType {
    MIDSEMA_SCOPETYPE_ROOT,
    MIDSEMA_SCOPETYPE_FUNC,
    MIDSEMA_SCOPETYPE_FUNC_PARAMS,
    MIDSEMA_SCOPETYPE_CLASS,
    MIDSEMA_SCOPETYPE_ENUM,
    MIDSEMA_SCOPETYPE_NAMESPACE,
    MIDSEMA_SCOPETYPE_COMPOUND_STMT,
    MIDSEMA_SCOPETYPE_TEMPLATE,
    MIDSEMA_SCOPETYPE_TEMPLATE_INST,
};

midgen_dynarray_struct_named(midsema_ScopePVec, struct midsema_Scope *);
struct midsema_Scope {
    struct midsema_IdentVec idents; // NOTE: be careful about ptr invalidation
    struct midsema_ScopePVec childs;
    struct midsema_Scope *parent;
    struct midpar_ASTNode *node;
    enum midsema_ScopeType type;
};

void midsema_Scope_deinit(struct midsema_Scope *self);
void midsema_copy_scope(struct midsema_Scope *dest,
                        const struct midsema_Scope *src,
                        struct midsema_Scope *dest_parent,
                        struct midpar_Allocators *allocs);
struct midsema_Scope midsema_create_empty_scope(enum midsema_ScopeType type,
                                                struct midsema_Scope *parent,
                                                struct midpar_ASTNode *node);

// RNCE - Root, Namespace, Class, or Enum
bool midsema_is_rnce_scope(enum midsema_ScopeType type);
// returns self if it itself is an RNCE scope
const struct midsema_Scope *
midsema_closest_rnce_scope_const(const struct midsema_Scope *self);
struct midsema_Scope *midsema_closest_rnce_scope(struct midsema_Scope *self);

const struct midsema_Scope *
midsema_closest_scope_of_type_const(const struct midsema_Scope *self,
                                    enum midsema_ScopeType type);
struct midsema_Scope *
midsema_closest_scope_of_type(struct midsema_Scope *self,
                              enum midsema_ScopeType type);

const struct midsema_Ident *
midsema_find_ident_const(const struct midsema_Scope *scope, const char *name);
struct midsema_Ident *midsema_find_ident(struct midsema_Scope *scope,
                                         const char *name);

bool midsema_is_type_name(const struct midsema_Scope *scope, const char *name);
bool midsema_is_namespace_name(const struct midsema_Scope *scope,
                               const char *name);
bool midsema_ident_type(struct midsema_Scope *scope,
                        const struct midsema_Ident *ident,
                        struct midpar_Type *out_type);
bool midsema_name_type(struct midsema_Scope *scope, const char *name,
                       struct midpar_Type *out_type);
// type of a type-name.
struct midpar_Type midsema_type_name_type(struct midsema_Scope *scope,
                                          const char *name);
struct midpar_Type midsema_tok_type(struct midsema_Scope *scope,
                                    const struct midlex_Token *tok);

// if the scope already has an identifier of the same name, nothing is added and
// the old identifier is returned
// ident->parent is changed to scope
struct midsema_Ident *midsema_add_ident(struct midsema_Scope *scope,
                                        struct midsema_Ident *ident);
// same as midsema_add_ident but adds a copy of the ident instead
// ident's copy's parent is changed to scope
struct midsema_Ident *midsema_add_ident_copy(struct midsema_Scope *scope,
                                             const struct midsema_Ident *ident,
                                             bool copy_ident_scopes,
                                             struct midpar_Allocators *allocs);
// returns 0 on success, returns 1 if the identifier already has a declaration
int32_t midsema_add_ident_def(struct midsema_Scope *scope, const char *name,
                              struct midpar_ASTNode *def);

// finds an RNCE scope in scope
// returns NULL if the scope name couldn't be resolved
const struct midsema_Scope *
midsema_resolve_scope_const(const char *name,
                            const struct midsema_Scope *scope);
struct midsema_Scope *midsema_resolve_scope(const char *name,
                                            struct midsema_Scope *scope);

// returns NULL if it doesn't have one
const char *midsema_scope_name(const struct midsema_Scope *scope);

void midsema_add_tmplt_params_to_scope(struct midsema_Scope *scope,
                                       const struct midsema_Scope *tmplt);

#ifdef __cplusplus
}
#endif
