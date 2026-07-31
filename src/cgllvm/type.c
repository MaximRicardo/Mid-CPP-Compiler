#include "type.h"
#include "dynstr.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "llvm_vecs.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/class.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "types.h"
#include <llvm-c-20/llvm-c/Core.h>
#include <llvm-c-20/llvm-c/Types.h>
#include <string.h>

static bool is_ptr(const struct MidParser_Type *type, bool ref_is_ptr)
{
    return MidParser_n_indir(type) > 0 ||
           (ref_is_ptr && (type->lv_ref || type->rv_ref)) ||
           type->spec == MIDPARSER_TYPESPEC_FPTR ||
           type->spec == MIDPARSER_TYPESPEC_NULLPTR;
}

LLVMTypeRef MidLLVM_convert_parser_type(const struct MidParser_Type *type,
                                       LLVMContextRef context, bool ref_is_ptr)
{
    if (is_ptr(type, ref_is_ptr))
        return LLVMPointerTypeInContext(context, 0);

    switch (type->spec) {
    case MIDPARSER_TYPESPEC_VOID:
        return LLVMVoidTypeInContext(context);

    case MIDPARSER_TYPESPEC_BOOL:
        return LLVMInt1TypeInContext(context);

    case MIDPARSER_TYPESPEC_CHAR:
    case MIDPARSER_TYPESPEC_SCHAR:
    case MIDPARSER_TYPESPEC_UCHAR:
        return LLVMIntTypeInContext(context, MidTypes_char_size * 8);

    case MIDPARSER_TYPESPEC_WCHAR:
        return LLVMIntTypeInContext(context, MidTypes_wchar_size * 8);

    case MIDPARSER_TYPESPEC_CHAR16:
        return LLVMIntTypeInContext(context, 16);

    case MIDPARSER_TYPESPEC_CHAR32:
        return LLVMIntTypeInContext(context, 32);

    case MIDPARSER_TYPESPEC_SHORT:
    case MIDPARSER_TYPESPEC_USHORT:
        return LLVMIntTypeInContext(context, MidTypes_short_size * 8);

    case MIDPARSER_TYPESPEC_INT:
    case MIDPARSER_TYPESPEC_UINT:
        return LLVMIntTypeInContext(context, MidTypes_int_size * 8);

    case MIDPARSER_TYPESPEC_LONG:
    case MIDPARSER_TYPESPEC_ULONG:
        return LLVMIntTypeInContext(context, MidTypes_long_size * 8);

    case MIDPARSER_TYPESPEC_LONGLONG:
    case MIDPARSER_TYPESPEC_ULONGLONG:
        return LLVMIntTypeInContext(context, MidTypes_longlong_size * 8);

    case MIDPARSER_TYPESPEC_FLOAT:
        return LLVMFloatTypeInContext(context);

    case MIDPARSER_TYPESPEC_DOUBLE:
        return LLVMDoubleTypeInContext(context);

    case MIDPARSER_TYPESPEC_LONGDOUBLE:
        return LLVMDoubleTypeInContext(context);

    case MIDPARSER_TYPESPEC_ARRAY:
        return LLVMArrayType2(
            MidLLVM_convert_parser_type(&type->array->elem, context, true),
            type->array->len);

    case MIDPARSER_TYPESPEC_CLASS:
        return MidLLVM_create_struct(
            &MidSema_deref_identptr(&type->named)->def->class_, context);

    default:
        MID_CRASH("converting this type is not supported");
    }
}

static void add_scopes_to_str(const struct MidSema_Scope *scope,
                              struct Mid_Dynstr *str)
{
    if (!scope->parent) {
        return;
    } else if (!MidSema_is_rnce_scope(scope->type)) {
        add_scopes_to_str(MidSema_closest_rnce_scope_const(scope), str);
        return;
    }

    auto next = MidSema_closest_rnce_scope_const(scope->parent);
    add_scopes_to_str(next, str);

    if (next->type != MIDSEMA_SCOPETYPE_ROOT)
        MidDynstr_append(str, "::");
    MidDynstr_append(str, MidSema_scope_name(scope));
}

static const struct MidSema_Scope *get_start_scope(const struct MidSema_Ident *ident)
{
    switch (ident->type) {
    case MIDSEMA_IDENTTYPE_CLASS:
        return MidParser_class_parent(&ident->decl->class_);

    case MIDSEMA_IDENTTYPE_ENUM:
        MID_CRASH("enums not supported yet");

    default:
        MID_CRASH("ident type not supported");
    }
}

char *MidLLVM_named_type_full_name(const struct MidSema_IdentPtr *named)
{
    struct Mid_Dynstr str = {};

    auto ident = MidSema_deref_identptr(named);
    add_scopes_to_str(get_start_scope(ident), &str);

    MidDynstr_append_printf(&str, "::%s", ident->name);

    return str.str;
}

struct MidLLVM_TypeRefVec
MidLLVM_class_to_struct_fields(const struct MidParser_Class *src,
                              LLVMContextRef context)
{
    struct MidLLVM_TypeRefVec ret = {};

    for (mid_isize i = 0; i < src->childs.len; ++i) {
        const struct MidParser_ASTNode *child = src->childs.arr[i];

        if (child->type != MIDPARSER_ASTNODETYPE_VAR_DECL)
            continue;

        for (mid_isize j = 0; j < child->var_decl.insts.len; ++j) {
            const struct MidParser_VarDeclInst *inst =
                child->var_decl.insts.arr[j];
            MidGen_dynpush(&ret,
                        MidLLVM_convert_parser_type(&inst->type, context, true));
        }
    }

    return ret;
}

mid_isize MidLLVM_class_field_to_struct_field_idx(const struct MidParser_Class *src,
                                               const char *name)
{
    mid_isize ret = 0;

    for (mid_isize i = 0; i < src->childs.len; ++i) {
        const struct MidParser_ASTNode *child = src->childs.arr[i];

        if (child->type != MIDPARSER_ASTNODETYPE_VAR_DECL)
            continue;

        for (mid_isize j = 0; j < child->var_decl.insts.len; ++j) {
            const struct MidParser_VarDeclInst *inst =
                child->var_decl.insts.arr[j];

            if (!strcmp(inst->name, name))
                return ret;

            ++ret;
        }
    }

    return -1;
}

LLVMTypeRef MidLLVM_create_struct(const struct MidParser_Class *src,
                                 LLVMContextRef context)
{
    struct MidLLVM_TypeRefVec fields =
        MidLLVM_class_to_struct_fields(src, context);

    auto ret = LLVMStructTypeInContext(context, fields.arr, fields.len, false);
    MidGen_dyndeinit(&fields);
    return ret;
}
