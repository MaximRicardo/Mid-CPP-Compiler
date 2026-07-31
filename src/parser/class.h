#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "sema/ident.h"
#include "sema/scope.h"

enum MidParser_ClassType {
    MIDPARSER_CLASSTYPE_CLASS,
    MIDPARSER_CLASSTYPE_STRUCT,
    MIDPARSER_CLASSTYPE_UNION,
};

enum MidParser_ClassAccess {
    MIDPARSER_CLASSACCESS_PUBLIC,
    MIDPARSER_CLASSACCESS_PRIVATE,
    MIDPARSER_CLASSACCESS_PROTECTED,
};

struct MidParser_Class;
MidGen_dynarray_struct_named(MidParser_ClassPVec, struct MidParser_Class *);

// classes, structs and unions
struct MidParser_Class {
    struct MidParser_ASTNodePVec childs;
    struct MidParser_ASTNodePVec pub_childs;  // public
    struct MidParser_ASTNodePVec priv_childs; // private
    struct MidParser_ASTNodePVec prot_childs; // protected
    struct MidParser_VarDecl *var;            // a class declaration can also act
                                           // as a variable declaration cuz why
                                           // tf not i guess.
                                           // class A {...} x, *y, *const z;
    const char *name;
    struct MidParser_ClassPVec supers;      // classes this class inherits from
    const struct MidLexer_Token *def_start; // the left curly '{'
    struct MidSema_IdentPtr ident;
    enum MidParser_ClassType type;
    bool has_def;
};

void MidParser_Class_deinit(struct MidParser_Class *self);
void MidParser_copy_class(struct MidParser_Class *dest,
                       const struct MidParser_Class *src,
                       struct MidSema_Scope *dest_scope,
                       struct MidParser_Allocators *allocs);
struct MidSema_Scope *MidParser_class_parent(const struct MidParser_Class *self);
// returns the end of the class
mid_isize MidParser_parse_class(struct MidParser_Class *self, struct MidSema_Scope *scope,
                           const struct MidLexer_Token *toks, mid_isize start,
                           bool skip_def, struct MidParser_Allocators *allocs,
                           struct MidDiag_DiagVec *diags);
void MidParser_parse_class_def(struct MidParser_Class *self,
                            const struct MidLexer_Token *toks,
                            struct MidSema_Scope *scope,
                            struct MidParser_Allocators *allocs,
                            struct MidDiag_DiagVec *diags);
bool MidParser_is_field_pub(const struct MidParser_Class *self,
                         const struct MidParser_ASTNode *child);
bool MidParser_is_field_priv(const struct MidParser_Class *self,
                          const struct MidParser_ASTNode *child);
bool MidParser_is_field_prot(const struct MidParser_Class *self,
                          const struct MidParser_ASTNode *child);
enum MidParser_ClassAccess MidParser_field_access(const struct MidParser_Class *self,
                                            const struct MidParser_ASTNode *child);
// returns the idx of the field in self->childs
mid_isize MidParser_find_field(const struct MidParser_Class *self, const char *name);
struct MidParser_FuncDeclPVec MidParser_class_ctors(const struct MidParser_Class *self);
