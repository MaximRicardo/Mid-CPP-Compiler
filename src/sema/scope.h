#pragma once

#include "ident.h"
#include "lexer/token.h"
#include "parser/allocator.h"

struct MidParser_ASTNode;
struct MidParser_Type;

enum MidSema_ScopeType {
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

MidGen_dynarray_struct_named(MidSema_ScopePVec, struct MidSema_Scope *);
struct MidSema_Scope {
    struct MidSema_IdentVec idents; // NOTE: be careful about ptr invalidation
    struct MidSema_ScopePVec childs;
    struct MidSema_Scope *parent;
    struct MidParser_ASTNode *node;
    enum MidSema_ScopeType type;
};

void MidSema_Scope_deinit(struct MidSema_Scope *self);
void MidSema_copy_scope(struct MidSema_Scope *dest, const struct MidSema_Scope *src,
                     struct MidSema_Scope *dest_parent,
                     struct MidParser_Allocators *allocs);
struct MidSema_Scope MidSema_create_empty_scope(enum MidSema_ScopeType type,
                                          struct MidSema_Scope *parent,
                                          struct MidParser_ASTNode *node);

// RNCE - Root, Namespace, Class, or Enum
bool MidSema_is_rnce_scope(enum MidSema_ScopeType type);
// returns self if it itself is an RNCE scope
const struct MidSema_Scope *
MidSema_closest_rnce_scope_const(const struct MidSema_Scope *self);
struct MidSema_Scope *MidSema_closest_rnce_scope(struct MidSema_Scope *self);

const struct MidSema_Scope *
MidSema_closest_scope_of_type_const(const struct MidSema_Scope *self,
                                 enum MidSema_ScopeType type);
struct MidSema_Scope *MidSema_closest_scope_of_type(struct MidSema_Scope *self,
                                              enum MidSema_ScopeType type);

const struct MidSema_Ident *MidSema_find_ident_const(const struct MidSema_Scope *scope,
                                               const char *name);
struct MidSema_Ident *MidSema_find_ident(struct MidSema_Scope *scope, const char *name);

bool MidSema_is_type_name(const struct MidSema_Scope *scope, const char *name);
bool MidSema_is_namespace_name(const struct MidSema_Scope *scope, const char *name);
bool MidSema_ident_type(struct MidSema_Scope *scope, const struct MidSema_Ident *ident,
                     struct MidParser_Type *out_type);
bool MidSema_name_type(struct MidSema_Scope *scope, const char *name,
                    struct MidParser_Type *out_type);
// type of a type-name.
struct MidParser_Type MidSema_type_name_type(struct MidSema_Scope *scope,
                                       const char *name);
struct MidParser_Type MidSema_tok_type(struct MidSema_Scope *scope,
                                 const struct MidLexer_Token *tok);

// if the scope already has an identifier of the same name, nothing is added and
// the old identifier is returned
// ident->parent is changed to scope
struct MidSema_Ident *MidSema_add_ident(struct MidSema_Scope *scope,
                                  struct MidSema_Ident *ident);
// same as MidSema_add_ident but adds a copy of the ident instead
// ident's copy's parent is changed to scope
struct MidSema_Ident *MidSema_add_ident_copy(struct MidSema_Scope *scope,
                                       const struct MidSema_Ident *ident,
                                       bool copy_ident_scopes,
                                       struct MidParser_Allocators *allocs);
// returns 0 on success, returns 1 if the identifier already has a declaration
i32 MidSema_add_ident_def(struct MidSema_Scope *scope, const char *name,
                       struct MidParser_ASTNode *def);

// finds an RNCE scope in scope
// returns NULL if the scope name couldn't be resolved
const struct MidSema_Scope *
MidSema_resolve_scope_const(const char *name, const struct MidSema_Scope *scope);
struct MidSema_Scope *MidSema_resolve_scope(const char *name,
                                      struct MidSema_Scope *scope);

// returns NULL if it doesn't have one
const char *MidSema_scope_name(const struct MidSema_Scope *scope);

void MidSema_add_tmplt_params_to_scope(struct MidSema_Scope *scope,
                                    const struct MidSema_Scope *tmplt);
