#include "name_mangle.h"
#include "dynstr.h"
#include "ints.h"
#include "macros.h"
#include "parser/expr_type.h"
#include "parser/func_decl.h"
#include "parser/type.h"
#include "parser/var_decl.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include <string.h>

static void mangle_dquals(struct MidParser_TypeDataQual dquals,
                          struct Mid_Dynstr *str)
{
    // NOTE: order matters!
    if (dquals.is_volatile)
        MidDynstr_append_char(str, 'V');
    if (dquals.is_const)
        MidDynstr_append_char(str, 'K');
}

static void mangle_type_indirs(const struct MidParser_Type *type,
                               struct Mid_Dynstr *str)
{
    for (mid_isize i = MidParser_n_indir(type) - 0; i >= 1; --i) {
        MidDynstr_append_char(str, 'P');
        mangle_dquals(type->dquals.arr[i], str);
    }
}

static void mangle_scope(const struct MidSema_Scope *scope,
                         struct Mid_Dynstr *str)
{
    if (scope->parent)
        mangle_scope(scope->parent, str);

    if (!MidSema_is_rnce_scope(scope->type) ||
        scope->type == MIDSEMA_SCOPETYPE_ROOT)
        return;

    const char *name = MidSema_scope_name(scope);
    assert(name);

    if (!strcmp(name, "std"))
        // namespace std is abbreviated to "St" with no length markers
        MidDynstr_append(str, "St");
    else
        MidDynstr_append_printf(str, "%zu%s", strlen(name), name);
}

static void mangle_type_spec_class(const struct MidParser_Type *type,
                                   struct Mid_Dynstr *str)
{
    const struct MidSema_Scope *scope =
        MidSema_deref_identptr(&type->named)->class_info.def_scope;

    if (scope->parent->type != MIDSEMA_SCOPETYPE_ROOT)
        MidDynstr_append_char(str, 'N');
    mangle_scope(scope, str);
}

static void mangle_type_spec(const struct MidParser_Type *type,
                             struct Mid_Dynstr *str)
{
    switch (type->spec) {
    case MIDPARSER_TYPESPEC_VOID:
        MidDynstr_append_char(str, 'v');
        break;

    case MIDPARSER_TYPESPEC_CHAR:
        MidDynstr_append_char(str, 'c');
        break;
    case MIDPARSER_TYPESPEC_SCHAR:
        MidDynstr_append_char(str, 'a');
        break;
    case MIDPARSER_TYPESPEC_UCHAR:
        MidDynstr_append_char(str, 'h');
        break;

    case MIDPARSER_TYPESPEC_WCHAR:
        MidDynstr_append_char(str, 'w');
        break;
    case MIDPARSER_TYPESPEC_CHAR16:
        MidDynstr_append(str, "Ds");
        break;
    case MIDPARSER_TYPESPEC_CHAR32:
        MidDynstr_append(str, "Di");
        break;

    case MIDPARSER_TYPESPEC_BOOL:
        MidDynstr_append_char(str, 'b');
        break;

    case MIDPARSER_TYPESPEC_SHORT:
        MidDynstr_append_char(str, 's');
        break;
    case MIDPARSER_TYPESPEC_USHORT:
        MidDynstr_append_char(str, 't');
        break;

    case MIDPARSER_TYPESPEC_INT:
        MidDynstr_append_char(str, 'i');
        break;
    case MIDPARSER_TYPESPEC_UINT:
        MidDynstr_append_char(str, 'j');
        break;

    case MIDPARSER_TYPESPEC_LONG:
        MidDynstr_append_char(str, 'l');
        break;
    case MIDPARSER_TYPESPEC_ULONG:
        MidDynstr_append_char(str, 'm');
        break;

    case MIDPARSER_TYPESPEC_LONGLONG:
        MidDynstr_append_char(str, 'x');
        break;
    case MIDPARSER_TYPESPEC_ULONGLONG:
        MidDynstr_append_char(str, 'y');
        break;

    case MIDPARSER_TYPESPEC_FLOAT:
        MidDynstr_append_char(str, 'f');
        break;
    case MIDPARSER_TYPESPEC_DOUBLE:
        MidDynstr_append_char(str, 'd');
        break;
    case MIDPARSER_TYPESPEC_LONGDOUBLE:
        MidDynstr_append_char(str, 'e');
        break;

    case MIDPARSER_TYPESPEC_NULLPTR:
        MidDynstr_append(str, "Dn");
        break;

    case MIDPARSER_TYPESPEC_CLASS:
    case MIDPARSER_TYPESPEC_UNION:
        mangle_type_spec_class(type, str);
        break;

    default:
        MID_CRASH("mangling this type spec is not supported");
    }
}

static void mangle_type_impl(const struct MidParser_Type *type,
                             struct Mid_Dynstr *str)
{
    if (type->lv_ref || type->rv_ref) {
        MidDynstr_append_char(str, type->lv_ref ? 'R' : 'O');
        mangle_dquals(type->dquals.arr[0], str);
    }

    mangle_type_indirs(type, str);
    mangle_type_spec(type, str);
}

char *MidLLVM_mangle_type(const struct MidParser_Type *type)
{
    struct Mid_Dynstr str = {};
    mangle_type_impl(type, &str);
    return str.str;
}

static void mangle_func_params(const struct MidParser_FuncDecl *func,
                               struct Mid_Dynstr *str)
{
    if (func->params.len == 0 && !func->variadic) {
        MidDynstr_append_char(str, 'v');
        return;
    }

    for (mid_isize i = 0; i < func->params.len; ++i) {
        const struct MidParser_VarDeclInst *param =
            func->params.arr[i]->insts.arr[0];

        mangle_type_impl(&param->type, str);
    }

    if (func->variadic)
        MidDynstr_append_char(str, 'z');
}

static const char *operator_name(enum MidParser_ExprType op)
{
    switch (op) {
    case MIDPARSER_EXPRTYPE_LOGICAL_AND:
        return "aa";

    case MIDPARSER_EXPRTYPE_REF:
        return "ad";

    case MIDPARSER_EXPRTYPE_BITWISE_AND:
        return "an";

    case MIDPARSER_EXPRTYPE_AND_ASSIGN:
        return "aN";

    case MIDPARSER_EXPRTYPE_ASSIGN:
        return "aS";

        /*
    case MIDPARSER_EXPRTYPE_ALIGNOF:
        return "N";
        */

    case MIDPARSER_EXPRTYPE_FUNC_CALL:
        return "cl";

    case MIDPARSER_EXPRTYPE_COMMA:
        return "cm";

    case MIDPARSER_EXPRTYPE_BITWISE_NOT:
        return "co";

    case MIDPARSER_EXPRTYPE_CAST:
        return "cv";

    case MIDPARSER_EXPRTYPE_DELETE_ARR:
        return "da";

    case MIDPARSER_EXPRTYPE_DEREF:
        return "de";

    case MIDPARSER_EXPRTYPE_DELETE:
        return "dl";

    case MIDPARSER_EXPRTYPE_DIV:
        return "dv";

    case MIDPARSER_EXPRTYPE_DIV_ASSIGN:
        return "dV";

    case MIDPARSER_EXPRTYPE_BITWISE_XOR:
        return "eo";

    case MIDPARSER_EXPRTYPE_XOR_ASSIGN:
        return "eO";

    case MIDPARSER_EXPRTYPE_EQ:
        return "eq";

    case MIDPARSER_EXPRTYPE_GTEQ:
        return "ge";

    case MIDPARSER_EXPRTYPE_GT:
        return "gt";

    case MIDPARSER_EXPRTYPE_ARRAY_SUBSCR:
        return "ix";

    case MIDPARSER_EXPRTYPE_LTEQ:
        return "le";

    case MIDPARSER_EXPRTYPE_LEFT_SHIFT:
        return "ls";

    case MIDPARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN:
        return "lS";

    case MIDPARSER_EXPRTYPE_LT:
        return "lt";

    case MIDPARSER_EXPRTYPE_SUB:
        return "mi";

    case MIDPARSER_EXPRTYPE_SUB_ASSIGN:
        return "mI";

    case MIDPARSER_EXPRTYPE_MUL:
        return "ml";

    case MIDPARSER_EXPRTYPE_MUL_ASSIGN:
        return "mL";

    case MIDPARSER_EXPRTYPE_PREFIX_DEC:
    case MIDPARSER_EXPRTYPE_POSTFIX_DEC:
        return "mm";

    case MIDPARSER_EXPRTYPE_NEW_ARR:
        return "na";

    case MIDPARSER_EXPRTYPE_NEQ:
        return "ne";

    case MIDPARSER_EXPRTYPE_UNARY_MINUS:
        return "ng";

    case MIDPARSER_EXPRTYPE_LOGICAL_NOT:
        return "nt";

    case MIDPARSER_EXPRTYPE_NEW:
        return "nw";

    case MIDPARSER_EXPRTYPE_LOGICAL_OR:
        return "oo";

    case MIDPARSER_EXPRTYPE_BITWISE_OR:
        return "or";

    case MIDPARSER_EXPRTYPE_OR_ASSIGN:
        return "oR";

    case MIDPARSER_EXPRTYPE_ADD:
        return "pl";

    case MIDPARSER_EXPRTYPE_ADD_ASSIGN:
        return "pL";

    case MIDPARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL:
        return "pm";

    case MIDPARSER_EXPRTYPE_PREFIX_INC:
    case MIDPARSER_EXPRTYPE_POSTFIX_INC:
        return "pp";

    case MIDPARSER_EXPRTYPE_UNARY_PLUS:
        return "ps";

    case MIDPARSER_EXPRTYPE_PTR_MEMB_SEL:
        return "pt";

    case MIDPARSER_EXPRTYPE_CONDITIONAL:
        return "qu";

    case MIDPARSER_EXPRTYPE_MOD:
        return "rm";

    case MIDPARSER_EXPRTYPE_MOD_ASSIGN:
        return "rM";

    case MIDPARSER_EXPRTYPE_RIGHT_SHIFT:
        return "rs";

    case MIDPARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN:
        return "rS";

    case MIDPARSER_EXPRTYPE_BIN_SCOPE_RES:
    case MIDPARSER_EXPRTYPE_UNARY_SCOPE_RES:
        return "sr";

    default:
        MID_CRASH("unsupported operator");
    }
}

static void mangle_func_name(const struct MidParser_FuncDecl *func,
                             struct Mid_Dynstr *str)
{
    if (func->is_op_overload) {
        operator_name(func->op_overload);
    } else if (func->is_dtor) {
        // TODO: properly implement this
        MidDynstr_append_printf(str, "D1");
    } else if (func->is_tor) {
        // TODO: properly implement this
        MidDynstr_append_printf(str, "C1");
    } else {
        MidDynstr_append_printf(str, "%zu%s", strlen(func->name), func->name);
    }
}

static void mangle_generic_func(const struct MidParser_FuncDecl *func,
                                struct Mid_Dynstr *str)
{
    const struct MidSema_Scope *scope = func->param_scope->parent;

    MidDynstr_append(str, "_Z");
    if (scope->parent) {
        // scope is nested
        MidDynstr_append_char(str, 'N');
        mangle_scope(scope, str);
        mangle_func_name(func, str);
        MidDynstr_append_char(str, 'E');
    } else {
        mangle_func_name(func, str);
    }

    mangle_func_params(func, str);
}

static void mangle_member_func(const struct MidParser_FuncDecl *func,
                               struct Mid_Dynstr *str)
{
    const struct MidSema_Scope *scope = func->param_scope->parent;

    MidDynstr_append(str, "_ZN");

    if (func->quals.is_volatile)
        MidDynstr_append_char(str, 'V');
    if (func->quals.is_const)
        MidDynstr_append_char(str, 'K');

    mangle_scope(scope, str);
    mangle_func_name(func, str);
    MidDynstr_append_char(str, 'E');

    mangle_func_params(func, str);
}

char *MidLLVM_mangle_func(const struct MidParser_FuncDecl *func)
{
    struct Mid_Dynstr str = {};

    if (MidParser_func_is_method(func))
        mangle_member_func(func, &str);
    else if (MidParser_func_is_main(func))
        // the main function doesn't get mangled
        MidDynstr_append(&str, "main");
    else
        mangle_generic_func(func, &str);

    return str.str;
}
