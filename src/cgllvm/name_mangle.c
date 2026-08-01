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

static void mangle_dquals(struct midpar_TypeDataQual dquals,
                          struct mid_Dynstr *str)
{
    // NOTE: order matters!
    if (dquals.is_volatile)
        midstr_append_char(str, 'V');
    if (dquals.is_const)
        midstr_append_char(str, 'K');
}

static void mangle_type_indirs(const struct midpar_Type *type,
                               struct mid_Dynstr *str)
{
    for (mid_isize i = midpar_n_indir(type) - 0; i >= 1; --i) {
        midstr_append_char(str, 'P');
        mangle_dquals(type->dquals.arr[i], str);
    }
}

static void mangle_scope(const struct midsema_Scope *scope,
                         struct mid_Dynstr *str)
{
    if (scope->parent)
        mangle_scope(scope->parent, str);

    if (!midsema_is_rnce_scope(scope->type) ||
        scope->type == MIDSEMA_SCOPETYPE_ROOT)
        return;

    const char *name = midsema_scope_name(scope);
    assert(name);

    if (!strcmp(name, "std"))
        // namespace std is abbreviated to "St" with no length markers
        midstr_append(str, "St");
    else
        midstr_append_printf(str, "%zu%s", strlen(name), name);
}

static void mangle_type_spec_class(const struct midpar_Type *type,
                                   struct mid_Dynstr *str)
{
    const struct midsema_Scope *scope =
        midsema_deref_identptr(&type->named)->class_info.def_scope;

    if (scope->parent->type != MIDSEMA_SCOPETYPE_ROOT)
        midstr_append_char(str, 'N');
    mangle_scope(scope, str);
}

static void mangle_type_spec(const struct midpar_Type *type,
                             struct mid_Dynstr *str)
{
    switch (type->spec) {
    case MIDPAR_TYPESPEC_VOID:
        midstr_append_char(str, 'v');
        break;

    case MIDPAR_TYPESPEC_CHAR:
        midstr_append_char(str, 'c');
        break;
    case MIDPAR_TYPESPEC_SCHAR:
        midstr_append_char(str, 'a');
        break;
    case MIDPAR_TYPESPEC_UCHAR:
        midstr_append_char(str, 'h');
        break;

    case MIDPAR_TYPESPEC_WCHAR:
        midstr_append_char(str, 'w');
        break;
    case MIDPAR_TYPESPEC_CHAR16:
        midstr_append(str, "Ds");
        break;
    case MIDPAR_TYPESPEC_CHAR32:
        midstr_append(str, "Di");
        break;

    case MIDPAR_TYPESPEC_BOOL:
        midstr_append_char(str, 'b');
        break;

    case MIDPAR_TYPESPEC_SHORT:
        midstr_append_char(str, 's');
        break;
    case MIDPAR_TYPESPEC_USHORT:
        midstr_append_char(str, 't');
        break;

    case MIDPAR_TYPESPEC_INT:
        midstr_append_char(str, 'i');
        break;
    case MIDPAR_TYPESPEC_UINT:
        midstr_append_char(str, 'j');
        break;

    case MIDPAR_TYPESPEC_LONG:
        midstr_append_char(str, 'l');
        break;
    case MIDPAR_TYPESPEC_ULONG:
        midstr_append_char(str, 'm');
        break;

    case MIDPAR_TYPESPEC_LONGLONG:
        midstr_append_char(str, 'x');
        break;
    case MIDPAR_TYPESPEC_ULONGLONG:
        midstr_append_char(str, 'y');
        break;

    case MIDPAR_TYPESPEC_FLOAT:
        midstr_append_char(str, 'f');
        break;
    case MIDPAR_TYPESPEC_DOUBLE:
        midstr_append_char(str, 'd');
        break;
    case MIDPAR_TYPESPEC_LONGDOUBLE:
        midstr_append_char(str, 'e');
        break;

    case MIDPAR_TYPESPEC_NULLPTR:
        midstr_append(str, "Dn");
        break;

    case MIDPAR_TYPESPEC_CLASS:
    case MIDPAR_TYPESPEC_UNION:
        mangle_type_spec_class(type, str);
        break;

    default:
        MID_CRASH("mangling this type spec is not supported");
    }
}

static void mangle_type_impl(const struct midpar_Type *type,
                             struct mid_Dynstr *str)
{
    if (type->lv_ref || type->rv_ref) {
        midstr_append_char(str, type->lv_ref ? 'R' : 'O');
        mangle_dquals(type->dquals.arr[0], str);
    }

    mangle_type_indirs(type, str);
    mangle_type_spec(type, str);
}

char *midllvm_mangle_type(const struct midpar_Type *type)
{
    struct mid_Dynstr str = {};
    mangle_type_impl(type, &str);
    return str.str;
}

static void mangle_func_params(const struct midpar_FuncDecl *func,
                               struct mid_Dynstr *str)
{
    if (func->params.len == 0 && !func->variadic) {
        midstr_append_char(str, 'v');
        return;
    }

    for (mid_isize i = 0; i < func->params.len; ++i) {
        const struct midpar_VarDeclInst *param =
            func->params.arr[i]->insts.arr[0];

        mangle_type_impl(&param->type, str);
    }

    if (func->variadic)
        midstr_append_char(str, 'z');
}

static const char *operator_name(enum midpar_ExprType op)
{
    switch (op) {
    case MIDPAR_EXPRTYPE_LOGICAL_AND:
        return "aa";

    case MIDPAR_EXPRTYPE_REF:
        return "ad";

    case MIDPAR_EXPRTYPE_BITWISE_AND:
        return "an";

    case MIDPAR_EXPRTYPE_AND_ASSIGN:
        return "aN";

    case MIDPAR_EXPRTYPE_ASSIGN:
        return "aS";

        /*
    case MIDPAR_EXPRTYPE_ALIGNOF:
        return "N";
        */

    case MIDPAR_EXPRTYPE_FUNC_CALL:
        return "cl";

    case MIDPAR_EXPRTYPE_COMMA:
        return "cm";

    case MIDPAR_EXPRTYPE_BITWISE_NOT:
        return "co";

    case MIDPAR_EXPRTYPE_CAST:
        return "cv";

    case MIDPAR_EXPRTYPE_DELETE_ARR:
        return "da";

    case MIDPAR_EXPRTYPE_DEREF:
        return "de";

    case MIDPAR_EXPRTYPE_DELETE:
        return "dl";

    case MIDPAR_EXPRTYPE_DIV:
        return "dv";

    case MIDPAR_EXPRTYPE_DIV_ASSIGN:
        return "dV";

    case MIDPAR_EXPRTYPE_BITWISE_XOR:
        return "eo";

    case MIDPAR_EXPRTYPE_XOR_ASSIGN:
        return "eO";

    case MIDPAR_EXPRTYPE_EQ:
        return "eq";

    case MIDPAR_EXPRTYPE_GTEQ:
        return "ge";

    case MIDPAR_EXPRTYPE_GT:
        return "gt";

    case MIDPAR_EXPRTYPE_ARRAY_SUBSCR:
        return "ix";

    case MIDPAR_EXPRTYPE_LTEQ:
        return "le";

    case MIDPAR_EXPRTYPE_LEFT_SHIFT:
        return "ls";

    case MIDPAR_EXPRTYPE_LEFT_SHIFT_ASSIGN:
        return "lS";

    case MIDPAR_EXPRTYPE_LT:
        return "lt";

    case MIDPAR_EXPRTYPE_SUB:
        return "mi";

    case MIDPAR_EXPRTYPE_SUB_ASSIGN:
        return "mI";

    case MIDPAR_EXPRTYPE_MUL:
        return "ml";

    case MIDPAR_EXPRTYPE_MUL_ASSIGN:
        return "mL";

    case MIDPAR_EXPRTYPE_PREFIX_DEC:
    case MIDPAR_EXPRTYPE_POSTFIX_DEC:
        return "mm";

    case MIDPAR_EXPRTYPE_NEW_ARR:
        return "na";

    case MIDPAR_EXPRTYPE_NEQ:
        return "ne";

    case MIDPAR_EXPRTYPE_UNARY_MINUS:
        return "ng";

    case MIDPAR_EXPRTYPE_LOGICAL_NOT:
        return "nt";

    case MIDPAR_EXPRTYPE_NEW:
        return "nw";

    case MIDPAR_EXPRTYPE_LOGICAL_OR:
        return "oo";

    case MIDPAR_EXPRTYPE_BITWISE_OR:
        return "or";

    case MIDPAR_EXPRTYPE_OR_ASSIGN:
        return "oR";

    case MIDPAR_EXPRTYPE_ADD:
        return "pl";

    case MIDPAR_EXPRTYPE_ADD_ASSIGN:
        return "pL";

    case MIDPAR_EXPRTYPE_PTR_TO_PTR_MEMB_SEL:
        return "pm";

    case MIDPAR_EXPRTYPE_PREFIX_INC:
    case MIDPAR_EXPRTYPE_POSTFIX_INC:
        return "pp";

    case MIDPAR_EXPRTYPE_UNARY_PLUS:
        return "ps";

    case MIDPAR_EXPRTYPE_PTR_MEMB_SEL:
        return "pt";

    case MIDPAR_EXPRTYPE_CONDITIONAL:
        return "qu";

    case MIDPAR_EXPRTYPE_MOD:
        return "rm";

    case MIDPAR_EXPRTYPE_MOD_ASSIGN:
        return "rM";

    case MIDPAR_EXPRTYPE_RIGHT_SHIFT:
        return "rs";

    case MIDPAR_EXPRTYPE_RIGHT_SHIFT_ASSIGN:
        return "rS";

    case MIDPAR_EXPRTYPE_BIN_SCOPE_RES:
    case MIDPAR_EXPRTYPE_UNARY_SCOPE_RES:
        return "sr";

    default:
        MID_CRASH("unsupported operator");
    }
}

static void mangle_func_name(const struct midpar_FuncDecl *func,
                             struct mid_Dynstr *str)
{
    if (func->is_op_overload) {
        operator_name(func->op_overload);
    } else if (func->is_dtor) {
        // TODO: properly implement this
        midstr_append_printf(str, "D1");
    } else if (func->is_tor) {
        // TODO: properly implement this
        midstr_append_printf(str, "C1");
    } else {
        midstr_append_printf(str, "%zu%s", strlen(func->name), func->name);
    }
}

static void mangle_generic_func(const struct midpar_FuncDecl *func,
                                struct mid_Dynstr *str)
{
    const struct midsema_Scope *scope = func->param_scope->parent;

    midstr_append(str, "_Z");
    if (scope->parent) {
        // scope is nested
        midstr_append_char(str, 'N');
        mangle_scope(scope, str);
        mangle_func_name(func, str);
        midstr_append_char(str, 'E');
    } else {
        mangle_func_name(func, str);
    }

    mangle_func_params(func, str);
}

static void mangle_member_func(const struct midpar_FuncDecl *func,
                               struct mid_Dynstr *str)
{
    const struct midsema_Scope *scope = func->param_scope->parent;

    midstr_append(str, "_ZN");

    if (func->quals.is_volatile)
        midstr_append_char(str, 'V');
    if (func->quals.is_const)
        midstr_append_char(str, 'K');

    mangle_scope(scope, str);
    mangle_func_name(func, str);
    midstr_append_char(str, 'E');

    mangle_func_params(func, str);
}

char *midllvm_mangle_func(const struct midpar_FuncDecl *func)
{
    struct mid_Dynstr str = {};

    if (midpar_func_is_method(func))
        mangle_member_func(func, &str);
    else if (midpar_func_is_main(func))
        // the main function doesn't get mangled
        midstr_append(&str, "main");
    else
        mangle_generic_func(func, &str);

    return str.str;
}
