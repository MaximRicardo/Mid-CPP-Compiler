#include "sema/expr_eval.h"
#include "apfloat.h"
#include "apint.h"
#include "literal.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/expr_type.h"
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
    auto res = midsema_eval_expr(&expr->info.args.arr[0], scope);

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
            midlit_TaggedValue_deinit(&res);
            res.kind = MIDLIT_VALUE_SIGNED_INT;
            res.v.i = midint_init(midtype_bool_size * 8, 1, true);
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

    default:
        MID_CRASH("constant folding expr type not supported");
    }

    return res;
}

struct midlit_TaggedValue midsema_eval_expr(const struct midpar_Expr *expr,
                                            const struct midsema_Scope *scope)
{
    assert(expr->typechecked);
    assert(expr->ret.squals.is_constexpr);

    if (midpar_is_unaryop(expr->type))
        return eval_unaryop(expr, scope);
    else
        return eval_leaf(expr, scope);
}
