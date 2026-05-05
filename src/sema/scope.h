#pragma once

#include "ident.h"
#include "lexer/token.h"
#include "parser/expr.h"

struct Parser_ASTNode;

enum Sema_ScopeType {
    SEMA_SCOPETYPE_ROOT,
    SEMA_SCOPETYPE_FUNC,
    SEMA_SCOPETYPE_CLASS,
    SEMA_SCOPETYPE_ENUM,
    SEMA_SCOPETYPE_NAMESPACE,
    SEMA_SCOPETYPE_COMPOUND_STMT,
};

gen_dynarray_struct_named(Sema_ScopePVec, struct Sema_Scope *);
struct Sema_Scope {
    struct Sema_ScopePVec childs;
    struct Sema_Scope *parent;
    struct Parser_ASTNode *node;
    struct Sema_IdentVec idents;
    enum Sema_ScopeType type;
};

void Sema_Scope_deinit(struct Sema_Scope *self);
const struct Sema_Ident *Sema_find_ident_const(const struct Sema_Scope *scope,
                                               const char *name);
struct Sema_Ident *Sema_find_ident(struct Sema_Scope *scope, const char *name);

bool Sema_is_type_name(const struct Sema_Scope *scope, const char *name);
// type of a type-name.
struct Parser_Type Sema_type_name_type(const struct Sema_Scope *scope,
                                       const char *name);
// type of a declared identifier
const struct Parser_Type *Sema_name_type_const(const struct Sema_Scope *scope,
                                               const char *name);
struct Parser_Type *Sema_name_type(struct Sema_Scope *scope, const char *name);
bool Sema_tok_is_type(const struct Sema_Scope *scope,
                      const struct Lexer_Token *tok);
struct Parser_Type Sema_tok_type(const struct Sema_Scope *scope,
                                 const struct Lexer_Token *tok);

// if the scope already has an identifier of the same name, nothing is added and
// the old identifier is returned
struct Sema_Ident *Sema_add_ident(struct Sema_Scope *scope,
                                  const struct Sema_Ident *ident);
// returns 0 on success, returns 1 if the identifier already has a declaration
i32 Sema_add_ident_def(struct Sema_Scope *scope, const char *name,
                       struct Parser_ASTNode *def);

struct Parser_ASTNodePVec Sema_op_overloads(struct Sema_Scope *scope,
                                            enum Parser_ExprType op);
