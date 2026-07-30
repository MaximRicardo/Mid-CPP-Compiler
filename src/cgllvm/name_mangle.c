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

static void mangle_dquals(struct Parser_TypeDataQual dquals, struct Dynstr *str)
{
    // NOTE: order matters!
    if (dquals.is_volatile)
        Dynstr_append_char(str, 'V');
    if (dquals.is_const)
        Dynstr_append_char(str, 'K');
}

static void mangle_type_indirs(const struct Parser_Type *type,
                               struct Dynstr *str)
{
    for (isize_t i = Parser_n_indir(type) - 0; i >= 1; --i) {
        Dynstr_append_char(str, 'P');
        mangle_dquals(type->dquals.arr[i], str);
    }
}

static void mangle_scope(const struct Sema_Scope *scope, struct Dynstr *str)
{
    if (scope->parent)
        mangle_scope(scope->parent, str);

    if (!Sema_is_rnce_scope(scope->type) || scope->type == SEMA_SCOPETYPE_ROOT)
        return;

    const char *name = Sema_scope_name(scope);
    assert(name);

    if (!strcmp(name, "std"))
        // namespace std is abbreviated to "St" with no length markers
        Dynstr_append(str, "St");
    else
        Dynstr_append_printf(str, "%zu%s", strlen(name), name);
}

static void mangle_type_spec_class(const struct Parser_Type *type,
                                   struct Dynstr *str)
{
    const struct Sema_Scope *scope =
        Sema_deref_identptr(&type->named)->class_info.def_scope;

    if (scope->parent->type != SEMA_SCOPETYPE_ROOT)
        Dynstr_append_char(str, 'N');
    mangle_scope(scope, str);
}

static void mangle_type_spec(const struct Parser_Type *type, struct Dynstr *str)
{
    switch (type->spec) {
    case PARSER_TYPESPEC_VOID:
        Dynstr_append_char(str, 'v');
        break;

    case PARSER_TYPESPEC_CHAR:
        Dynstr_append_char(str, 'c');
        break;
    case PARSER_TYPESPEC_SCHAR:
        Dynstr_append_char(str, 'a');
        break;
    case PARSER_TYPESPEC_UCHAR:
        Dynstr_append_char(str, 'h');
        break;

    case PARSER_TYPESPEC_WCHAR:
        Dynstr_append_char(str, 'w');
        break;
    case PARSER_TYPESPEC_CHAR16:
        Dynstr_append(str, "Ds");
        break;
    case PARSER_TYPESPEC_CHAR32:
        Dynstr_append(str, "Di");
        break;

    case PARSER_TYPESPEC_BOOL:
        Dynstr_append_char(str, 'b');
        break;

    case PARSER_TYPESPEC_SHORT:
        Dynstr_append_char(str, 's');
        break;
    case PARSER_TYPESPEC_USHORT:
        Dynstr_append_char(str, 't');
        break;

    case PARSER_TYPESPEC_INT:
        Dynstr_append_char(str, 'i');
        break;
    case PARSER_TYPESPEC_UINT:
        Dynstr_append_char(str, 'j');
        break;

    case PARSER_TYPESPEC_LONG:
        Dynstr_append_char(str, 'l');
        break;
    case PARSER_TYPESPEC_ULONG:
        Dynstr_append_char(str, 'm');
        break;

    case PARSER_TYPESPEC_LONGLONG:
        Dynstr_append_char(str, 'x');
        break;
    case PARSER_TYPESPEC_ULONGLONG:
        Dynstr_append_char(str, 'y');
        break;

    case PARSER_TYPESPEC_FLOAT:
        Dynstr_append_char(str, 'f');
        break;
    case PARSER_TYPESPEC_DOUBLE:
        Dynstr_append_char(str, 'd');
        break;
    case PARSER_TYPESPEC_LONGDOUBLE:
        Dynstr_append_char(str, 'e');
        break;

    case PARSER_TYPESPEC_NULLPTR:
        Dynstr_append(str, "Dn");
        break;

    case PARSER_TYPESPEC_CLASS:
    case PARSER_TYPESPEC_UNION:
        mangle_type_spec_class(type, str);
        break;

    default:
        CRASH("mangling this type spec is not supported");
    }
}

static void mangle_type_impl(const struct Parser_Type *type, struct Dynstr *str)
{
    if (type->lv_ref || type->rv_ref) {
        Dynstr_append_char(str, type->lv_ref ? 'R' : 'O');
        mangle_dquals(type->dquals.arr[0], str);
    }

    mangle_type_indirs(type, str);
    mangle_type_spec(type, str);
}

char *CGLLVM_mangle_type(const struct Parser_Type *type)
{
    struct Dynstr str = {};
    mangle_type_impl(type, &str);
    return str.str;
}

static void mangle_func_params(const struct Parser_FuncDecl *func,
                               struct Dynstr *str)
{
    if (func->params.len == 0 && !func->variadic) {
        Dynstr_append_char(str, 'v');
        return;
    }

    for (isize_t i = 0; i < func->params.len; ++i) {
        const struct Parser_VarDeclInst *param =
            func->params.arr[i]->insts.arr[0];

        mangle_type_impl(&param->type, str);
    }

    if (func->variadic)
        Dynstr_append_char(str, 'z');
}

static const char *operator_name(enum Parser_ExprType op)
{
    switch (op) {
    case PARSER_EXPRTYPE_LOGICAL_AND:
        return "aa";

    case PARSER_EXPRTYPE_REF:
        return "ad";

    case PARSER_EXPRTYPE_BITWISE_AND:
        return "an";

    case PARSER_EXPRTYPE_AND_ASSIGN:
        return "aN";

    case PARSER_EXPRTYPE_ASSIGN:
        return "aS";

        /*
    case PARSER_EXPRTYPE_ALIGNOF:
        return "N";
        */

    case PARSER_EXPRTYPE_FUNC_CALL:
        return "cl";

    case PARSER_EXPRTYPE_COMMA:
        return "cm";

    case PARSER_EXPRTYPE_BITWISE_NOT:
        return "co";

    case PARSER_EXPRTYPE_CAST:
        return "cv";

    case PARSER_EXPRTYPE_DELETE_ARR:
        return "da";

    case PARSER_EXPRTYPE_DEREF:
        return "de";

    case PARSER_EXPRTYPE_DELETE:
        return "dl";

    case PARSER_EXPRTYPE_DIV:
        return "dv";

    case PARSER_EXPRTYPE_DIV_ASSIGN:
        return "dV";

    case PARSER_EXPRTYPE_BITWISE_XOR:
        return "eo";

    case PARSER_EXPRTYPE_XOR_ASSIGN:
        return "eO";

    case PARSER_EXPRTYPE_EQ:
        return "eq";

    case PARSER_EXPRTYPE_GTEQ:
        return "ge";

    case PARSER_EXPRTYPE_GT:
        return "gt";

    case PARSER_EXPRTYPE_ARRAY_SUBSCR:
        return "ix";

    case PARSER_EXPRTYPE_LTEQ:
        return "le";

    case PARSER_EXPRTYPE_LEFT_SHIFT:
        return "ls";

    case PARSER_EXPRTYPE_LEFT_SHIFT_ASSIGN:
        return "lS";

    case PARSER_EXPRTYPE_LT:
        return "lt";

    case PARSER_EXPRTYPE_SUB:
        return "mi";

    case PARSER_EXPRTYPE_SUB_ASSIGN:
        return "mI";

    case PARSER_EXPRTYPE_MUL:
        return "ml";

    case PARSER_EXPRTYPE_MUL_ASSIGN:
        return "mL";

    case PARSER_EXPRTYPE_PREFIX_DEC:
    case PARSER_EXPRTYPE_POSTFIX_DEC:
        return "mm";

    case PARSER_EXPRTYPE_NEW_ARR:
        return "na";

    case PARSER_EXPRTYPE_NEQ:
        return "ne";

    case PARSER_EXPRTYPE_UNARY_MINUS:
        return "ng";

    case PARSER_EXPRTYPE_LOGICAL_NOT:
        return "nt";

    case PARSER_EXPRTYPE_NEW:
        return "nw";

    case PARSER_EXPRTYPE_LOGICAL_OR:
        return "oo";

    case PARSER_EXPRTYPE_BITWISE_OR:
        return "or";

    case PARSER_EXPRTYPE_OR_ASSIGN:
        return "oR";

    case PARSER_EXPRTYPE_ADD:
        return "pl";

    case PARSER_EXPRTYPE_ADD_ASSIGN:
        return "pL";

    case PARSER_EXPRTYPE_PTR_TO_PTR_MEMB_SEL:
        return "pm";

    case PARSER_EXPRTYPE_PREFIX_INC:
    case PARSER_EXPRTYPE_POSTFIX_INC:
        return "pp";

    case PARSER_EXPRTYPE_UNARY_PLUS:
        return "ps";

    case PARSER_EXPRTYPE_PTR_MEMB_SEL:
        return "pt";

    case PARSER_EXPRTYPE_CONDITIONAL:
        return "qu";

    case PARSER_EXPRTYPE_MOD:
        return "rm";

    case PARSER_EXPRTYPE_MOD_ASSIGN:
        return "rM";

    case PARSER_EXPRTYPE_RIGHT_SHIFT:
        return "rs";

    case PARSER_EXPRTYPE_RIGHT_SHIFT_ASSIGN:
        return "rS";

    case PARSER_EXPRTYPE_BIN_SCOPE_RES:
    case PARSER_EXPRTYPE_UNARY_SCOPE_RES:
        return "sr";

    default:
        CRASH("unsupported operator");
    }
}

static void mangle_func_name(const struct Parser_FuncDecl *func,
                             struct Dynstr *str)
{
    if (func->is_op_overload) {
        operator_name(func->op_overload);
    } else if (func->is_dtor) {
        // TODO: properly implement this
        Dynstr_append_printf(str, "D1");
    } else if (func->is_tor) {
        // TODO: properly implement this
        Dynstr_append_printf(str, "C1");
    } else {
        Dynstr_append_printf(str, "%zu%s", strlen(func->name), func->name);
    }
}

static void mangle_generic_func(const struct Parser_FuncDecl *func,
                                struct Dynstr *str)
{
    const struct Sema_Scope *scope = func->param_scope->parent;

    Dynstr_append(str, "_Z");
    if (scope->parent) {
        // scope is nested
        Dynstr_append_char(str, 'N');
        mangle_scope(scope, str);
        mangle_func_name(func, str);
        Dynstr_append_char(str, 'E');
    } else {
        mangle_func_name(func, str);
    }

    mangle_func_params(func, str);
}

static void mangle_member_func(const struct Parser_FuncDecl *func,
                               struct Dynstr *str)
{
    const struct Sema_Scope *scope = func->param_scope->parent;

    Dynstr_append(str, "_ZN");

    if (func->quals.is_volatile)
        Dynstr_append_char(str, 'V');
    if (func->quals.is_const)
        Dynstr_append_char(str, 'K');

    mangle_scope(scope, str);
    mangle_func_name(func, str);
    Dynstr_append_char(str, 'E');

    mangle_func_params(func, str);
}

char *CGLLVM_mangle_func(const struct Parser_FuncDecl *func)
{
    struct Dynstr str = {};

    if (Parser_func_is_method(func))
        mangle_member_func(func, &str);
    else if (Parser_func_is_main(func))
        // the main function doesn't get mangled
        Dynstr_append(&str, "main");
    else
        mangle_generic_func(func, &str);

    return str.str;
}
