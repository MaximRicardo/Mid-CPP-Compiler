#pragma once

#include "parser/class.h"

#ifdef __cplusplus
extern "C" {
#endif

bool midsema_is_field_pub(const struct midpar_Class *self,
                          const struct midpar_ASTNode *child);
bool midsema_is_field_priv(const struct midpar_Class *self,
                           const struct midpar_ASTNode *child);
bool midsema_is_field_prot(const struct midpar_Class *self,
                           const struct midpar_ASTNode *child);
enum midpar_ClassAccess
midsema_field_access(const struct midpar_Class *self,
                     const struct midpar_ASTNode *child);
// returns the idx of the field in self->childs
mid_isize midsema_find_field(const struct midpar_Class *self, const char *name);
struct midpar_FuncDecl *
midsema_class_default_ctor(const struct midpar_Class *self);
struct midpar_FuncDeclPVec midsema_class_ctors(const struct midpar_Class *self);
struct midpar_FuncDecl *midsema_class_dtor(const struct midpar_Class *self);
bool midsema_has_explicit_ctors(const struct midpar_Class *self);
bool midsema_has_user_provided_ctors(const struct midpar_Class *self);
bool midsema_has_user_provided_dtor(const struct midpar_Class *self);
bool midsema_has_trivial_dtor(const struct midpar_Class *self);
// is the class a valid literal type?
bool midsema_class_is_literal(const struct midpar_Class *self);
bool midsema_class_is_aggregate(const struct midpar_Class *self);
bool midsema_has_inherited_ctors(const struct midpar_Class *self);
bool midsema_has_virt_methods(const struct midpar_Class *self);
// ignores non-static members
bool midsema_has_default_memb_initializers(const struct midpar_Class *self);
// a union has a variant member if it has a non-static member with a trivial
// default ctor
bool midsema_union_has_variant_member(const struct midpar_Class *self);
bool midsema_has_default_ctor(const struct midpar_Class *self);
bool midsema_class_is_trivially_constructible(const struct midpar_Class *self);
bool midsema_is_ctor_trivial(const struct midpar_FuncDecl *ctor);
bool midsema_has_trivial_default_ctor(const struct midpar_Class *self);

#ifdef __cplusplus
}
#endif
