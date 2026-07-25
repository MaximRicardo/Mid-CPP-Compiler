#pragma once

#include "diag.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "sema/ident.h"
#include "sema/scope.h"

enum Parser_ClassType {
    PARSER_CLASSTYPE_CLASS,
    PARSER_CLASSTYPE_STRUCT,
    PARSER_CLASSTYPE_UNION,
};

enum Parser_ClassAccess {
    PARSER_CLASSACCESS_PUBLIC,
    PARSER_CLASSACCESS_PRIVATE,
    PARSER_CLASSACCESS_PROTECTED,
};

// classes, structs and unions
struct Parser_Class {
    struct Parser_ASTNodePVec childs;
    struct Parser_ASTNodePVec pub_childs;  // public
    struct Parser_ASTNodePVec priv_childs; // private
    struct Parser_ASTNodePVec prot_childs; // protected
    struct Parser_ASTNode *var_decl;       // a class declaration can also act
                                           // as a variable declaration cuz why
                                           // tf not i guess.
                                           // class A {...} x, *y, *const z;
    const char *name;
    struct Parser_ASTNodePVec supers;    // classes this class inherits from
    const struct Lexer_Token *def_start; // the left curly '{'
    struct Sema_IdentPtr ident;
    enum Parser_ClassType type;
    bool has_def;
};

void Parser_Class_deinit(struct Parser_Class *self);
void Parser_copy_class(struct Parser_ASTNode *dest,
                       const struct Parser_ASTNode *src,
                       struct Sema_Scope *dest_scope,
                       struct Parser_Allocators *allocs);
struct Sema_Scope *Parser_class_parent(const struct Parser_Class *self);
// returns the end of the class
isize_t Parser_parse_class(struct Parser_ASTNode *node,
                           struct Sema_Scope *scope,
                           const struct Lexer_Token *toks, isize_t start,
                           bool skip_def, struct Parser_Allocators *allocs,
                           struct DiagVec *diags);
void Parser_parse_class_def(struct Parser_ASTNode *node,
                            const struct Lexer_Token *toks,
                            struct Sema_Scope *scope,
                            struct Parser_Allocators *allocs,
                            struct DiagVec *diags);
bool Parser_is_field_pub(const struct Parser_Class *self,
                         const struct Parser_ASTNode *child);
bool Parser_is_field_priv(const struct Parser_Class *self,
                          const struct Parser_ASTNode *child);
bool Parser_is_field_prot(const struct Parser_Class *self,
                          const struct Parser_ASTNode *child);
enum Parser_ClassAccess Parser_field_access(const struct Parser_Class *self,
                                            const struct Parser_ASTNode *child);
// returns the idx of the field in self->childs
isize_t Parser_find_field(const struct Parser_Class *self, const char *name);
struct Parser_ASTNodePVec Parser_class_ctors(const struct Parser_Class *self);
