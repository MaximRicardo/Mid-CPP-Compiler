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

static bool is_ptr(const struct Parser_Type *type)
{
    return Parser_n_indir(type) > 0 || type->lv_ref || type->rv_ref ||
           type->spec == PARSER_TYPESPEC_FPTR ||
           type->spec == PARSER_TYPESPEC_NULLPTR;
}

LLVMTypeRef CGLLVM_convert_parser_type(const struct Parser_Type *type,
                                       LLVMContextRef context)
{
    if (is_ptr(type))
        return LLVMPointerTypeInContext(context, 0);

    switch (type->spec) {
    case PARSER_TYPESPEC_VOID:
        return LLVMVoidTypeInContext(context);

    case PARSER_TYPESPEC_BOOL:
        return LLVMInt1TypeInContext(context);

    case PARSER_TYPESPEC_CHAR:
    case PARSER_TYPESPEC_SCHAR:
    case PARSER_TYPESPEC_UCHAR:
        return LLVMIntTypeInContext(context, Types_char_size * 8);

    case PARSER_TYPESPEC_WCHAR:
        return LLVMIntTypeInContext(context, Types_wchar_size * 8);

    case PARSER_TYPESPEC_CHAR16:
        return LLVMIntTypeInContext(context, 16);

    case PARSER_TYPESPEC_CHAR32:
        return LLVMIntTypeInContext(context, 32);

    case PARSER_TYPESPEC_SHORT:
    case PARSER_TYPESPEC_USHORT:
        return LLVMIntTypeInContext(context, Types_short_size * 8);

    case PARSER_TYPESPEC_INT:
    case PARSER_TYPESPEC_UINT:
        return LLVMIntTypeInContext(context, Types_int_size * 8);

    case PARSER_TYPESPEC_LONG:
    case PARSER_TYPESPEC_ULONG:
        return LLVMIntTypeInContext(context, Types_long_size * 8);

    case PARSER_TYPESPEC_LONGLONG:
    case PARSER_TYPESPEC_ULONGLONG:
        return LLVMIntTypeInContext(context, Types_longlong_size * 8);

    case PARSER_TYPESPEC_FLOAT:
        return LLVMFloatTypeInContext(context);

    case PARSER_TYPESPEC_DOUBLE:
        return LLVMDoubleTypeInContext(context);

    case PARSER_TYPESPEC_LONGDOUBLE:
        return LLVMDoubleTypeInContext(context);

    case PARSER_TYPESPEC_ARRAY:
        return LLVMArrayType2(
            CGLLVM_convert_parser_type(&type->array->elem, context),
            type->array->len);

    case PARSER_TYPESPEC_CLASS:
        return CGLLVM_create_struct(
            &Parser_named_type_ident(&type->named)->def->class_, context);

    default:
        CRASH("converting this type is not supported");
    }
}

static void add_scopes_to_str(const struct Sema_Scope *scope,
                              struct Dynstr *str)
{
    if (!scope->parent) {
        return;
    } else if (!Sema_is_rnce_scope(scope->type)) {
        add_scopes_to_str(Sema_closest_rnce_scope_const(scope), str);
        return;
    }

    auto next = Sema_closest_rnce_scope_const(scope->parent);
    add_scopes_to_str(next, str);

    if (next->type != SEMA_SCOPETYPE_ROOT)
        Dynstr_append(str, "::");
    Dynstr_append(str, Sema_scope_name(scope));
}

static const struct Sema_Scope *get_start_scope(const struct Sema_Ident *ident)
{
    switch (ident->type) {
    case SEMA_IDENTTYPE_CLASS:
        return ident->decl->class_.parent;

    case SEMA_IDENTTYPE_ENUM:
        CRASH("enums not supported yet");

    default:
        CRASH("ident type not supported");
    }
}

char *CGLLVM_named_type_full_name(const struct Parser_TypeNamed *named)
{
    struct Dynstr str = {};

    auto ident = Parser_named_type_ident(named);
    add_scopes_to_str(get_start_scope(ident), &str);

    Dynstr_append_printf(&str, "::%s", ident->name);

    return str.str;
}

struct CGLLVM_TypeRefVec
CGLLVM_class_to_struct_fields(const struct Parser_Class *src,
                              LLVMContextRef context)
{
    struct CGLLVM_TypeRefVec ret = {};

    for (isize_t i = 0; i < src->childs.len; ++i) {
        const struct Parser_ASTNode *child = src->childs.arr[i];

        if (child->type != PARSER_ASTNODETYPE_VAR_DECL)
            continue;

        for (isize_t j = 0; j < child->var_decl.insts.len; ++i) {
            const struct Parser_VarDeclInst *inst =
                &child->var_decl.insts.arr[i];
            gen_dynpush(&ret, CGLLVM_convert_parser_type(&inst->type, context));
        }
    }

    return ret;
}

LLVMTypeRef CGLLVM_create_struct(const struct Parser_Class *src,
                                 LLVMContextRef context)
{
    struct CGLLVM_TypeRefVec fields =
        CGLLVM_class_to_struct_fields(src, context);

    auto ret = LLVMStructTypeInContext(context, fields.arr, fields.len, false);
    gen_dyndeinit(&fields);
    return ret;
}
