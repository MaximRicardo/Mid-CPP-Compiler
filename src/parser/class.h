#pragma once

#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/allocator.h"
#include "parser/astvec.h"
#include "sema/ident.h"
#include "sema/scope.h"

enum midpar_ClassType {
    MIDPAR_CLASSTYPE_CLASS,
    MIDPAR_CLASSTYPE_STRUCT,
    MIDPAR_CLASSTYPE_UNION,
};

enum midpar_ClassAccess {
    MIDPAR_CLASSACCESS_PUBLIC,
    MIDPAR_CLASSACCESS_PRIVATE,
    MIDPAR_CLASSACCESS_PROTECTED,
};

struct midpar_Class;
midgen_dynarray_struct_named(midpar_ClassPVec, struct midpar_Class *);

// classes, structs and unions
struct midpar_Class {
    struct midpar_ASTNodePVec childs;
    struct midpar_ASTNodePVec pub_childs;  // public
    struct midpar_ASTNodePVec priv_childs; // private
    struct midpar_ASTNodePVec prot_childs; // protected
    struct midpar_VarDecl *var;            // a class declaration can also act
                                           // as a variable declaration cuz why
                                           // tf not i guess.
                                           // class A {...} x, *y, *const z;
    const char *name;
    struct midpar_ClassPVec supers;       // classes this class inherits from
    const struct midlex_Token *def_start; // the left curly '{'
    struct midsema_IdentPtr ident;
    enum midpar_ClassType type;
    bool has_def;
};

void midpar_Class_deinit(struct midpar_Class *self);
void midpar_copy_class(struct midpar_Class *dest,
                       const struct midpar_Class *src,
                       struct midsema_Scope *dest_scope,
                       struct midpar_Allocators *allocs);
struct midsema_Scope *midpar_class_parent(const struct midpar_Class *self);
// returns the end of the class
mid_isize midpar_parse_class(struct midpar_Class *self,
                             struct midsema_Scope *scope,
                             const struct midlex_Token *toks, mid_isize start,
                             bool skip_def, struct midpar_Allocators *allocs,
                             struct mid_DiagVec *diags);
void midpar_parse_class_def(struct midpar_Class *self,
                            const struct midlex_Token *toks,
                            struct midsema_Scope *scope,
                            struct midpar_Allocators *allocs,
                            struct mid_DiagVec *diags);
bool midpar_is_field_pub(const struct midpar_Class *self,
                         const struct midpar_ASTNode *child);
bool midpar_is_field_priv(const struct midpar_Class *self,
                          const struct midpar_ASTNode *child);
bool midpar_is_field_prot(const struct midpar_Class *self,
                          const struct midpar_ASTNode *child);
enum midpar_ClassAccess midpar_field_access(const struct midpar_Class *self,
                                            const struct midpar_ASTNode *child);
// returns the idx of the field in self->childs
mid_isize midpar_find_field(const struct midpar_Class *self, const char *name);
struct midpar_FuncDeclPVec midpar_class_ctors(const struct midpar_Class *self);
