#include "sema/expr_eval.h"
#include "apfloat.h"
#include "apint.h"
#include "literal.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/expr_type.h"
#include "parser/type.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "types.h"

static bool op_always_constexpr(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_SIZEOF;
}

static bool leaf_expr_is_constexpr(const struct midpar_Expr *expr)
{
    if (expr->type == MIDPAR_EXPRTYPE_IDENTIFIER)
        return expr->ret.squals.is_constexpr;
    else if (midpar_is_numlit(expr->type) || midpar_is_strlit(expr->type))
        return true;
    else
        return false;
}

bool midsema_expr_is_constexpr(struct midpar_Expr *expr)
{
    assert(expr->typechecked);

    if (expr->ret.squals.is_constexpr)
        return true;

    // TODO: implement constexpr function calls
    if (expr->type == MIDPAR_EXPRTYPE_FUNC_CALL)
        return false;

    bool is_constexpr = false;

    if (op_always_constexpr(expr->type)) {
        is_constexpr = true;
    } else if (midpar_is_op(expr->type)) {
        // TODO: add support for overloaded operators
        assert(!expr->overloaded);

        is_constexpr = true;
        for (int i = 0; i < expr->info.args.len; ++i) {
            if (!midsema_expr_is_constexpr(&expr->info.args.arr[i])) {
                is_constexpr = false;
                break;
            }
        }
    } else {
        is_constexpr = leaf_expr_is_constexpr(expr);
    }

    expr->ret.squals.is_constexpr = is_constexpr;
    return is_constexpr;
}

static struct midlit_TaggedValue
get_ident_value(const struct midpar_Expr *expr,
                const struct midsema_Scope *scope)
{
    auto ident = midsema_find_ident_const(scope, expr->info.ident);
    assert(ident);

    if (ident->type != MIDSEMA_IDENTTYPE_VAR)
        MID_CRASH("getting the value of this identifier type isn't supported");
    else
        return midsema_eval_expr(ident->decl->var_inst.init.expr, scope);
}

static struct midlit_TaggedValue eval_leaf(const struct midpar_Expr *expr,
                                           const struct midsema_Scope *scope)
{
    if (midpar_is_numlit(expr->type) || midpar_is_strlit(expr->type))
        return midlit_copy_value(&expr->info.val);
    else if (expr->type == MIDPAR_EXPRTYPE_IDENTIFIER)
        return get_ident_value(expr, scope);
    else
        MID_CRASH("can't get value of this expr");
}

static struct midlit_TaggedValue eval_unaryop(const struct midpar_Expr *expr,
                                              const struct midsema_Scope *scope)
{
    const struct midpar_Expr *child = &expr->info.args.arr[0];

    struct midlit_TaggedValue res;
    if (expr->type != MIDPAR_EXPRTYPE_SIZEOF)
        res = midsema_eval_expr(child, scope);

    bool is_integral = res.kind == MIDLIT_VALUE_SIGNED_INT ||
                       res.kind == MIDLIT_VALUE_UNSIGNED_INT;

    switch (expr->type) {
    case MIDPAR_EXPRTYPE_BITWISE_NOT:
        assert(is_integral);
        midint_not(&res.v.i);
        break;

    case MIDPAR_EXPRTYPE_LOGICAL_NOT:
        if (is_integral) {
            midint_assign_uimm(&res.v.i, !midint_is_zero(&res.v.i));
        } else if (res.kind == MIDLIT_VALUE_FLOAT) {
            bool is_zero = midflt_is_zero(&res.v.flt);
            enum midflt_Kind kind = res.v.flt.kind;
            enum midflt_Rounding rounding = midflt_get_rounding(&res.v.flt);
            mid_APFloat_deinit(&res.v.flt);
            res.v.flt = midflt_init(!is_zero, kind, rounding);
        } else {
            // !"asdf" is always false
            midlit_TaggedValue_deinit(&res);
            res.kind = MIDLIT_VALUE_SIGNED_INT;
            res.v.i = midint_init(midtype_bool_size * 8, 0, true);
        }
        break;

    case MIDPAR_EXPRTYPE_UNARY_PLUS:
        break;

    case MIDPAR_EXPRTYPE_UNARY_MINUS:
        if (is_integral)
            midint_negate(&res.v.i);
        else
            midflt_flip_sign(&res.v.flt);
        break;

    case MIDPAR_EXPRTYPE_SIZEOF:
        res.v.i = midpar_sizeof_type(&child->ret);
        res.kind = MIDLIT_VALUE_UNSIGNED_INT;
        break;

    default:
        MID_CRASH("constant folding expr type not supported");
    }

    return res;
}

static void convert_value(struct midlit_TaggedValue *val,
                          const struct midpar_Type *target)
{
    enum midlit_ValueKind new_kind = midpar_type_lit_value_kind(target);
    int_least64_t target_width = midpar_typespec_size(target->spec) * 8;
    enum midflt_Kind target_kind = midpar_is_floating_typespec(target->spec)
                                       ? midpar_get_flt_kind(target->spec)
                                       : 0;

    switch (val->kind) {
    case MIDLIT_VALUE_SIGNED_INT:
    case MIDLIT_VALUE_UNSIGNED_INT:
        switch (new_kind) {
        case MIDLIT_VALUE_SIGNED_INT:
            midint_ext(&val->v.i, target_width, true);
            break;

        case MIDLIT_VALUE_UNSIGNED_INT:
            midint_ext(&val->v.i, target_width, false);
            break;

        case MIDLIT_VALUE_FLOAT: {
            auto tmp = val->kind == MIDLIT_VALUE_SIGNED_INT
                           ? midflt_init_sint(&val->v.i, target_width,
                                              midtype_default_rmode)
                           : midflt_init_uint(&val->v.i, target_width,
                                              midtype_default_rmode);
            mid_APInt_deinit(&val->v.i);
            val->v.flt = tmp;
        } break;

        case MIDLIT_VALUE_STR:
            MID_CRASH("can't convert integer to string");
        }
        break;

    case MIDLIT_VALUE_FLOAT:
        switch (new_kind) {
        case MIDLIT_VALUE_SIGNED_INT:
        case MIDLIT_VALUE_UNSIGNED_INT: {
            auto tmp = midflt_to_sint(&val->v.flt);
            mid_APFloat_deinit(&val->v.flt);
            val->v.i = tmp;
            midint_ext(&val->v.i, target_width,
                       new_kind == MIDLIT_VALUE_SIGNED_INT);
        } break;

        case MIDLIT_VALUE_FLOAT:
            midflt_change_kind(&val->v.flt, target_kind);
            break;

        case MIDLIT_VALUE_STR:
            MID_CRASH("can't convert float to string");
        }
        break;

    case MIDLIT_VALUE_STR:
        MID_CRASH("can't convert strings");
    }

    val->kind = new_kind;
}

static struct midlit_TaggedValue
eval_arith_binop(const struct midpar_Expr *expr, struct midlit_TaggedValue *lhs,
                 struct midlit_TaggedValue *rhs)
{
    enum midlit_ValueKind res_kind = midpar_type_lit_value_kind(&expr->ret);
    bool is_integral = res_kind == MIDLIT_VALUE_SIGNED_INT ||
                       res_kind == MIDLIT_VALUE_UNSIGNED_INT;

    if (res_kind == MIDLIT_VALUE_STR)
        MID_CRASH("can't const fold arithmetic operations on strings");

    convert_value(lhs, &expr->ret);
    convert_value(rhs, &expr->ret);

    struct midlit_TaggedValue res = {.kind = res_kind};

    switch (expr->type) {
    case MIDPAR_EXPRTYPE_ADD:
        if (is_integral)
            res.v.i = midint_nip_add(&lhs->v.i, &rhs->v.i);
        else
            res.v.flt = midflt_nip_add(&lhs->v.flt, &rhs->v.flt);
        break;

    case MIDPAR_EXPRTYPE_SUB:
        if (is_integral)
            res.v.i = midint_nip_sub(&lhs->v.i, &rhs->v.i);
        else
            res.v.flt = midflt_nip_sub(&lhs->v.flt, &rhs->v.flt);
        break;

    case MIDPAR_EXPRTYPE_MUL:
        if (is_integral)
            res.v.i = midint_nip_mul(&lhs->v.i, &rhs->v.i);
        else
            res.v.flt = midflt_nip_mul(&lhs->v.flt, &rhs->v.flt);
        break;

    case MIDPAR_EXPRTYPE_DIV:
        if (res.kind == MIDLIT_VALUE_SIGNED_INT)
            res.v.i = midint_nip_sdiv(&lhs->v.i, &rhs->v.i);
        else if (res.kind == MIDLIT_VALUE_UNSIGNED_INT)
            res.v.i = midint_nip_udiv(&lhs->v.i, &rhs->v.i);
        else
            res.v.flt = midflt_nip_mul(&lhs->v.flt, &rhs->v.flt);
        break;

    case MIDPAR_EXPRTYPE_MOD:
        if (res.kind == MIDLIT_VALUE_SIGNED_INT)
            res.v.i = midint_nip_srem(&lhs->v.i, &rhs->v.i);
        else if (res.kind == MIDLIT_VALUE_UNSIGNED_INT)
            res.v.i = midint_nip_urem(&lhs->v.i, &rhs->v.i);
        else
            MID_CRASH("modulo of floats not implemented yet");
        break;

    case MIDPAR_EXPRTYPE_LEFT_SHIFT:
        if (is_integral)
            res.v.i = midint_nip_shl(&lhs->v.i, &rhs->v.i);
        else
            MID_CRASH("can't left shift a float");

    case MIDPAR_EXPRTYPE_RIGHT_SHIFT:
        if (res.kind == MIDLIT_VALUE_SIGNED_INT)
            res.v.i = midint_nip_ashr(&lhs->v.i, &rhs->v.i);
        else if (res.kind == MIDLIT_VALUE_UNSIGNED_INT)
            res.v.i = midint_nip_lshr(&lhs->v.i, &rhs->v.i);
        else
            MID_CRASH("can't right shift a float");
        break;

    case MIDPAR_EXPRTYPE_BITWISE_AND:
        if (is_integral)
            res.v.i = midint_nip_and(&lhs->v.i, &rhs->v.i);
        else
            MID_CRASH("can't bitwise AND a float");
        break;

    case MIDPAR_EXPRTYPE_BITWISE_XOR:
        if (is_integral)
            res.v.i = midint_nip_xor(&lhs->v.i, &rhs->v.i);
        else
            MID_CRASH("can't bitwise XOR a float");
        break;

    case MIDPAR_EXPRTYPE_BITWISE_OR:
        if (is_integral)
            res.v.i = midint_nip_or(&lhs->v.i, &rhs->v.i);
        else
            MID_CRASH("can't bitwise OR a float");
        break;

    default:
        MID_CRASH("expr type is not a binary arithmetic operator");
    }

    return res;
}

static struct midlit_TaggedValue eval_binop(const struct midpar_Expr *expr,
                                            const struct midsema_Scope *scope)
{
    const struct midpar_Expr *lhs = &expr->info.args.arr[0];
    const struct midpar_Expr *rhs = &expr->info.args.arr[1];

    struct midlit_TaggedValue res_lhs = midsema_eval_expr(lhs, scope);
    struct midlit_TaggedValue res_rhs = midsema_eval_expr(rhs, scope);

    struct midlit_TaggedValue res;

    if (midpar_is_arith_op(expr->type))
        res = eval_arith_binop(expr, &res_lhs, &res_rhs);
    else
        MID_CRASH("constant folding expr type not supported");

    midlit_TaggedValue_deinit(&res_lhs);
    midlit_TaggedValue_deinit(&res_rhs);
    return res;
}

struct midlit_TaggedValue midsema_eval_expr(const struct midpar_Expr *expr,
                                            const struct midsema_Scope *scope)
{
    assert(expr->typechecked);
    assert(expr->ret.squals.is_constexpr);

    if (midpar_is_unaryop(expr->type))
        return eval_unaryop(expr, scope);
    else if (midpar_is_binop(expr->type))
        return eval_binop(expr, scope);
    else
        return eval_leaf(expr, scope);
}
