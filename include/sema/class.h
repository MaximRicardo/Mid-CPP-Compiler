#pragma once

#include "literal.h"
#include "parser/class.h"
#include "parser/var_decl.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO: add checks for nested classes with a built in variable declaration

bool midsema_is_field_pub(const struct midpar_Class *self,
                          const struct midpar_ASTNode *child);
bool midsema_is_field_priv(const struct midpar_Class *self,
                           const struct midpar_ASTNode *child);
bool midsema_is_field_prot(const struct midpar_Class *self,
                           const struct midpar_ASTNode *child);
enum midpar_ClassAccess
midsema_field_access(const struct midpar_Class *self,
                     const struct midpar_ASTNode *child);
// returns a midpar_VarDeclInst if the field is a variable, and a
// midpar_FuncDecl if the field is a method
struct midpar_ASTNode *midsema_find_field(const struct midpar_Class *self,
                                          const char *name);
// returns an array of every non-static data field in self
struct midpar_VarDeclInstPVec
midsema_nonstatic_dfields(const struct midpar_Class *self);
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
bool midsema_class_has_constexpr_default_ctor(const struct midpar_Class *self);
// returns true on success, returns false on failure. fails if the field doesn't
// have a default value or if the default value isn't constexpr
// out_val        - can't be NULL.
bool midsema_field_default_value(const struct midpar_Class *self,
                                 const char *name,
                                 struct midlit_TaggedValue *out_val);

#ifdef __cplusplus
}
#endif
