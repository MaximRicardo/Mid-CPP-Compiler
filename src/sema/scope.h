#pragma once

#include "ident.h"
#include "lexer/token.h"
#include "parser/allocator.h"

struct Parser_ASTNode;

enum Sema_ScopeType {
    SEMA_SCOPETYPE_ROOT,
    SEMA_SCOPETYPE_FUNC,
    SEMA_SCOPETYPE_FUNC_PARAMS,
    SEMA_SCOPETYPE_CLASS,
    SEMA_SCOPETYPE_ENUM,
    SEMA_SCOPETYPE_NAMESPACE,
    SEMA_SCOPETYPE_COMPOUND_STMT,
    SEMA_SCOPETYPE_TEMPLATE,
};

gen_dynarray_struct_named(Sema_ScopePVec, struct Sema_Scope *);
struct Sema_Scope {
    struct Sema_IdentVec idents; // NOTE: be careful about ptr invalidation
    struct Sema_ScopePVec childs;
    struct Sema_Scope *parent;
    struct Parser_ASTNode *node;
    enum Sema_ScopeType type;
};

void Sema_Scope_deinit(struct Sema_Scope *self);
void Sema_copy_scope(struct Sema_Scope *dest, const struct Sema_Scope *src,
                     struct Sema_Scope *dest_parent,
                     struct Parser_Allocators *allocs);

// RNCE - Root, Namespace, Class, or Enum
bool Sema_is_rnce_scope(enum Sema_ScopeType type);
// returns self if it itself is an RNCE scope
const struct Sema_Scope *
Sema_closest_rnce_scope_const(const struct Sema_Scope *self);
struct Sema_Scope *Sema_closest_rnce_scope(struct Sema_Scope *self);

const struct Sema_Scope *
Sema_closest_scope_of_type_const(const struct Sema_Scope *self,
                                 enum Sema_ScopeType type);
struct Sema_Scope *Sema_closest_scope_of_type(struct Sema_Scope *self,
                                              enum Sema_ScopeType type);

const struct Sema_Ident *
Sema_find_ident_const(const struct Sema_Scope *scope, const char *name,
                      const struct Sema_Scope **out_ident_scope);
struct Sema_Ident *Sema_find_ident(struct Sema_Scope *scope, const char *name,
                                   struct Sema_Scope **out_ident_scope);

bool Sema_is_type_name(const struct Sema_Scope *scope, const char *name);
bool Sema_is_namespace_name(const struct Sema_Scope *scope, const char *name);
bool Sema_ident_type(struct Sema_Scope *scope, const struct Sema_Ident *ident,
                     struct Parser_Type *out_type);
bool Sema_name_type(struct Sema_Scope *scope, const char *name,
                    struct Parser_Type *out_type);
// type of a type-name.
struct Parser_Type Sema_type_name_type(struct Sema_Scope *scope,
                                       const char *name);
struct Parser_Type Sema_tok_type(struct Sema_Scope *scope,
                                 const struct Lexer_Token *tok);

// if the scope already has an identifier of the same name, nothing is added and
// the old identifier is returned
struct Sema_Ident *Sema_add_ident(struct Sema_Scope *scope,
                                  struct Sema_Ident *ident);
// same as Sema_add_ident but adds a copy of the ident instead
struct Sema_Ident *Sema_add_ident_copy(struct Sema_Scope *scope,
                                       const struct Sema_Ident *ident,
                                       struct Parser_Allocators *allocs);
// returns 0 on success, returns 1 if the identifier already has a declaration
i32 Sema_add_ident_def(struct Sema_Scope *scope, const char *name,
                       struct Parser_ASTNode *def);

// finds an RNCE scope in scope
// returns NULL if the scope name couldn't be resolved
const struct Sema_Scope *
Sema_resolve_scope_const(const char *name, const struct Sema_Scope *scope);
struct Sema_Scope *Sema_resolve_scope(const char *name,
                                      struct Sema_Scope *scope);

// returns NULL if it doesn't have one
const char *Sema_scope_name(const struct Sema_Scope *scope);

void Sema_add_tmplt_params_to_scope(struct Sema_Scope *scope,
                                    const struct Sema_Scope *tmplt);
