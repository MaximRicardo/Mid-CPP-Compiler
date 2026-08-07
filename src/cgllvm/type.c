#include "cgllvm/type.h"
#include "cgllvm/llvm_vecs.h"
#include "dynstr.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/class.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/type.h"
#include "types.h"
#include <llvm-c-20/llvm-c/Core.h>
#include <llvm-c-20/llvm-c/Types.h>
#include <string.h>

static bool is_ptr(const struct midpar_Type *type, bool ref_is_ptr)
{
    return midsema_n_indir(type) > 0 ||
           (ref_is_ptr && (type->lv_ref || type->rv_ref)) ||
           type->spec == MIDPAR_TYPESPEC_FPTR ||
           type->spec == MIDPAR_TYPESPEC_NULLPTR;
}

LLVMTypeRef midllvm_convert_parser_type(const struct midpar_Type *type,
                                        LLVMContextRef context, bool ref_is_ptr)
{
    if (is_ptr(type, ref_is_ptr))
        return LLVMPointerTypeInContext(context, 0);

    switch (type->spec) {
    case MIDPAR_TYPESPEC_VOID:
        return LLVMVoidTypeInContext(context);

    case MIDPAR_TYPESPEC_BOOL:
        return LLVMInt1TypeInContext(context);

    case MIDPAR_TYPESPEC_CHAR:
    case MIDPAR_TYPESPEC_SCHAR:
    case MIDPAR_TYPESPEC_UCHAR:
        return LLVMIntTypeInContext(context, midtype_char_size * 8);

    case MIDPAR_TYPESPEC_WCHAR:
        return LLVMIntTypeInContext(context, midtype_wchar_size * 8);

    case MIDPAR_TYPESPEC_CHAR16:
        return LLVMIntTypeInContext(context, 16);

    case MIDPAR_TYPESPEC_CHAR32:
        return LLVMIntTypeInContext(context, 32);

    case MIDPAR_TYPESPEC_SHORT:
    case MIDPAR_TYPESPEC_USHORT:
        return LLVMIntTypeInContext(context, midtype_short_size * 8);

    case MIDPAR_TYPESPEC_INT:
    case MIDPAR_TYPESPEC_UINT:
        return LLVMIntTypeInContext(context, midtype_int_size * 8);

    case MIDPAR_TYPESPEC_LONG:
    case MIDPAR_TYPESPEC_ULONG:
        return LLVMIntTypeInContext(context, midtype_long_size * 8);

    case MIDPAR_TYPESPEC_LONGLONG:
    case MIDPAR_TYPESPEC_ULONGLONG:
        return LLVMIntTypeInContext(context, midtype_longlong_size * 8);

    case MIDPAR_TYPESPEC_FLOAT:
        return LLVMFloatTypeInContext(context);

    case MIDPAR_TYPESPEC_DOUBLE:
        return LLVMDoubleTypeInContext(context);

    case MIDPAR_TYPESPEC_LONGDOUBLE:
        return LLVMDoubleTypeInContext(context);

    case MIDPAR_TYPESPEC_ARRAY:
        return LLVMArrayType2(
            midllvm_convert_parser_type(&type->array->elem, context, true),
            type->array->len);

    case MIDPAR_TYPESPEC_CLASS:
        return midllvm_create_struct(
            &midsema_deref_identptr(&type->named)->def->class_, context);

    default:
        MID_CRASH("converting this type is not supported");
    }
}

static void add_scopes_to_str(const struct midsema_Scope *scope,
                              struct mid_Dynstr *str)
{
    if (!scope->parent) {
        return;
    } else if (!midsema_is_rnce_scope(scope->type)) {
        add_scopes_to_str(midsema_closest_rnce_scope_const(scope), str);
        return;
    }

    auto next = midsema_closest_rnce_scope_const(scope->parent);
    add_scopes_to_str(next, str);

    if (next->type != MIDSEMA_SCOPETYPE_ROOT)
        midstr_append(str, "::");
    midstr_append(str, midsema_scope_name(scope));
}

static const struct midsema_Scope *
get_start_scope(const struct midsema_Ident *ident)
{
    switch (ident->type) {
    case MIDSEMA_IDENTTYPE_CLASS:
        return midpar_class_parent(&ident->decl->class_);

    case MIDSEMA_IDENTTYPE_ENUM:
        MID_CRASH("enums not supported yet");

    default:
        MID_CRASH("ident type not supported");
    }
}

char *midllvm_named_type_full_name(const struct midsema_IdentPtr *named)
{
    struct mid_Dynstr str = {};

    auto ident = midsema_deref_identptr(named);
    add_scopes_to_str(get_start_scope(ident), &str);

    midstr_append_printf(&str, "::%s", ident->name);

    return str.str;
}

struct midllvm_TypeRefVec
midllvm_class_to_struct_fields(const struct midpar_Class *src,
                               LLVMContextRef context)
{
    struct midllvm_TypeRefVec ret = {};

    for (mid_isize i = 0; i < src->childs.len; ++i) {
        const struct midpar_ASTNode *child = src->childs.arr[i];

        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL)
            continue;

        for (mid_isize j = 0; j < child->var_decl.insts.len; ++j) {
            const struct midpar_VarDeclInst *inst =
                child->var_decl.insts.arr[j];
            midgen_dynpush(
                &ret, midllvm_convert_parser_type(&inst->type, context, true));
        }
    }

    return ret;
}

mid_isize
midllvm_class_field_to_struct_field_idx(const struct midpar_Class *src,
                                        const char *name)
{
    mid_isize ret = 0;

    for (mid_isize i = 0; i < src->childs.len; ++i) {
        const struct midpar_ASTNode *child = src->childs.arr[i];

        if (child->type != MIDPAR_ASTNODETYPE_VAR_DECL)
            continue;

        for (mid_isize j = 0; j < child->var_decl.insts.len; ++j) {
            const struct midpar_VarDeclInst *inst =
                child->var_decl.insts.arr[j];

            if (!strcmp(inst->name, name))
                return ret;

            ++ret;
        }
    }

    return -1;
}

LLVMTypeRef midllvm_create_struct(const struct midpar_Class *src,
                                  LLVMContextRef context)
{
    struct midllvm_TypeRefVec fields =
        midllvm_class_to_struct_fields(src, context);

    auto ret = LLVMStructTypeInContext(context, fields.arr, fields.len, false);
    midgen_dyndeinit(&fields);
    return ret;
}
