#include "sema/expr_eval.h"
#include "apfloat.h"
#include "apint.h"
#include "cmd.h"
#include "generics/dynarray.h"
#include "literal.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/expr_type.h"
#include "parser/type.h"
#include "sema/expr.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/type.h"
#include "types.h"

// a deinit queue is used to extend the life time of temporaries until the end
// of the expression
static struct midlit_TaggedValue
eval_expr(const struct midpar_Expr *expr, const struct midsema_Scope *scope,
          struct midlit_TaggedValueVec *deinit_queue);

static bool op_always_constant(enum midpar_ExprType type)
{
    return type == MIDPAR_EXPRTYPE_SIZEOF;
}

static bool leaf_expr_is_constant(const struct midpar_Expr *expr)
{
    if (expr->type == MIDPAR_EXPRTYPE_IDENTIFIER)
        return expr->ret.squals.is_constexpr;
    else if (midsema_is_numlit(expr->type) || midsema_is_strlit(expr->type))
        return true;
    else
        return false;
}

void midsema_set_expr_constant_flag(struct midpar_Expr *expr)
{
    if (expr->typechecked)
        MID_CRASH("expression has already been typechecked");

    if (expr->constant) {
        return;
    } else if (expr->ret.spec == MIDPAR_TYPESPEC_UNKNOWN ||
               expr->ret.spec == MIDPAR_TYPESPEC_TEMPLATED) {
        expr->constant = false;
        return;
    } else if (midsema_op_has_side_effects(expr->type)) {
        expr->constant = false;
        return;
    }

    // TODO: implement constexpr function calls
    if (expr->type == MIDPAR_EXPRTYPE_FUNC_CALL) {
        expr->constant = false;
        return;
    }

    bool is_constant = false;

    if (op_always_constant(expr->type)) {
        is_constant = true;
    } else if (midsema_is_memb_sel(expr->type)) {
        is_constant = expr->info.args.arr[0].constant;
    } else if (midsema_expr_uses_args(expr->type)) {
        // TODO: add support for overloaded operators
        assert(!expr->overloaded);

        is_constant = true;
        for (int i = 0; i < expr->info.args.len; ++i) {
            struct midpar_Expr *arg = &expr->info.args.arr[i];
            if (!arg->constant) {
                is_constant = false;
                break;
            }
        }
    } else {
        is_constant = leaf_expr_is_constant(expr);
    }

    expr->constant = is_constant;
}

static struct midlit_TaggedValue *
get_ident_value_raw(const struct midpar_Expr *expr,
                    const struct midsema_Scope *scope)
{
    auto ident = midsema_find_ident_const(scope, expr->info.ident);
    assert(ident);

    const struct midpar_VarDeclInst *inst = &ident->decl->var_inst;
    if (!inst->constexpr_val)
        MID_CRASH("can't get the value of this identifier");

    return inst->constexpr_val;
}

// fetches a copy of the identifier's value
static struct midlit_TaggedValue
get_ident_value(const struct midpar_Expr *expr,
                const struct midsema_Scope *scope)
{
    return midlit_copy_value(get_ident_value_raw(expr, scope));
}

static struct midlit_TaggedValue create_nullptr_val()
{
    return (struct midlit_TaggedValue){.kind = MIDLIT_VALUE_PTR,
                                       .v.ptr = midlit_null_ptr()};
}

static struct midlit_TaggedValue eval_leaf(const struct midpar_Expr *expr,
                                           const struct midsema_Scope *scope)
{
    if (expr->type == MIDPAR_EXPRTYPE_NULLPTR_LIT)
        return create_nullptr_val();
    else if (midsema_is_numlit(expr->type) || midsema_is_strlit(expr->type))
        return midlit_copy_value(&expr->info.val);
    else if (expr->type == MIDPAR_EXPRTYPE_IDENTIFIER)
        return get_ident_value(expr, scope);
    else
        MID_CRASH("can't get value of this expr");
}

static struct midlit_TaggedValue eval_ref_op(const struct midpar_Expr *expr,
                                             const struct midsema_Scope *scope)
{
    const struct midpar_Expr *child = &expr->info.args.arr[0];
    assert(child->type == MIDPAR_EXPRTYPE_IDENTIFIER);

    return midlit_ref_val(get_ident_value_raw(child, scope));
}

static struct midlit_TaggedValue
eval_unaryop(const struct midpar_Expr *expr, const struct midsema_Scope *scope,
             struct midlit_TaggedValueVec *deinit_queue)
{
    if (expr->type == MIDPAR_EXPRTYPE_REF)
        return eval_ref_op(expr, scope);

    const struct midpar_Expr *child = &expr->info.args.arr[0];

    struct midlit_TaggedValue res;
    if (expr->type != MIDPAR_EXPRTYPE_SIZEOF)
        res = eval_expr(child, scope, deinit_queue);

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
            assert(res.kind == MIDLIT_VALUE_STR);
            // !"asdf" is always false
            midgen_dynpush(deinit_queue, res);
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
        res.v.i = midsema_sizeof_type(&child->ret);
        res.kind = MIDLIT_VALUE_UNSIGNED_INT;
        break;

    case MIDPAR_EXPRTYPE_REF: {
        midgen_dynpush(deinit_queue, res);
        res = midlit_ref_val(&res);
        break;
    }

    case MIDPAR_EXPRTYPE_DEREF: {
        struct midlit_TaggedValue *val = midlit_deref_ptr(&res.v.ptr);
        if (!val)
            MID_CRASH("can not dereference ptr");
        midgen_dynpush(deinit_queue, res);
        res = midlit_copy_value(val);
        break;
    }

    default:
        MID_CRASH("constant folding expr type not supported");
    }

    return res;
}

static void convert_value(struct midlit_TaggedValue *val,
                          const struct midpar_Type *target)
{
    enum midlit_ValueKind new_kind = midsema_type_lit_value_kind(target);
    int_least64_t target_width = midsema_typespec_size(target->spec) * 8;
    enum midflt_Kind target_kind = midsema_is_floating_typespec(target->spec)
                                       ? midsema_get_flt_kind(target->spec)
                                       : -1;

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
                           ? midflt_init_sint(&val->v.i, target_kind,
                                              midcmd_get_fpu()->rmode)
                           : midflt_init_uint(&val->v.i, target_kind,
                                              midcmd_get_fpu()->rmode);
            mid_APInt_deinit(&val->v.i);
            val->v.flt = tmp;
        } break;

        case MIDLIT_VALUE_STR:
            MID_CRASH("can't convert integer to string");

        case MIDLIT_VALUE_ARRAY:
            MID_CRASH("can't convert integer to array");

        case MIDLIT_VALUE_STRUCT:
            MID_CRASH("can't convert integer to struct");

        case MIDLIT_VALUE_UNION:
            MID_CRASH("can't convert integer to union");

        case MIDLIT_VALUE_PTR:
            MID_CRASH("can't convert integer to ptr");
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

        case MIDLIT_VALUE_ARRAY:
            MID_CRASH("can't convert float to array");

        case MIDLIT_VALUE_STRUCT:
            MID_CRASH("can't convert float to struct");

        case MIDLIT_VALUE_UNION:
            MID_CRASH("can't convert float to union");

        case MIDLIT_VALUE_PTR:
            MID_CRASH("can't convert float to ptr");
        }
        break;

    case MIDLIT_VALUE_STR:
        if (new_kind != MIDLIT_VALUE_STR)
            MID_CRASH("can't convert strings");
        break;

    case MIDLIT_VALUE_ARRAY:
        if (new_kind != MIDLIT_VALUE_ARRAY)
            MID_CRASH("can't convert structs");
        break;

    case MIDLIT_VALUE_STRUCT:
        if (new_kind != MIDLIT_VALUE_STRUCT)
            MID_CRASH("can't convert structs");
        break;

    case MIDLIT_VALUE_UNION:
        if (new_kind != MIDLIT_VALUE_UNION)
            MID_CRASH("can't convert unions");
        break;

    case MIDLIT_VALUE_PTR:
        if (new_kind != MIDLIT_VALUE_PTR)
            MID_CRASH("can't convert ptrs");
        break;
    }

    val->kind = new_kind;
}

static struct midlit_TaggedValue ptr_to_val(struct midlit_Ptr *self)
{
    return (struct midlit_TaggedValue){.v.ptr = *self,
                                       .kind = MIDLIT_VALUE_PTR};
}

static struct midlit_TaggedValue
eval_arith_ptr_binop(const struct midpar_Expr *expr,
                     const struct midlit_TaggedValue *lhs,
                     const struct midlit_TaggedValue *rhs,
                     struct midlit_TaggedValueVec *deinit_queue)
{
    const struct midlit_TaggedValue *ptr =
        lhs->kind == MIDLIT_VALUE_PTR ? lhs : rhs;
    const struct midlit_TaggedValue *off =
        lhs->kind == MIDLIT_VALUE_PTR ? rhs : lhs;

    assert(off->kind == MIDLIT_VALUE_SIGNED_INT ||
           off->kind == MIDLIT_VALUE_UNSIGNED_INT);

    struct midlit_TaggedValue res = {};
    if (expr->type != MIDPAR_EXPRTYPE_ARRAY_SUBSCR)
        res = midlit_copy_value(ptr);

    int_least64_t off_val = off->kind == MIDLIT_VALUE_UNSIGNED_INT
                                ? midint_to_uint(&off->v.i)
                                : midint_to_sint(&off->v.i);

    switch (expr->type) {
    case MIDPAR_EXPRTYPE_ADD:
        if (!midlit_inc_ptr(&res.v.ptr, off_val))
            MID_CRASH("failed to inc ptr");
        break;

    case MIDPAR_EXPRTYPE_SUB:
        if (!midlit_dec_ptr(&res.v.ptr, off_val))
            MID_CRASH("failed to dec ptr");
        break;

    case MIDPAR_EXPRTYPE_ARRAY_SUBSCR: {
        struct midlit_Ptr tmp = midlit_copy_ptr(&ptr->v.ptr);
        if (!midlit_inc_ptr(&tmp, off_val))
            MID_CRASH("failed to offset ptr");
        res = midlit_copy_value(midlit_deref_ptr(&tmp));
        midgen_dynpush(deinit_queue, ptr_to_val(&tmp));
    }

    default:
        MID_CRASH("expr type is not a valid binary ptr arithmetic operator");
    }

    return res;
}

static struct midlit_TaggedValue
eval_arith_arr_binop(const struct midpar_Expr *expr,
                     const struct midlit_TaggedValue *lhs,
                     const struct midlit_TaggedValue *rhs,
                     struct midlit_TaggedValueVec *deinit_queue)
{
    const struct midlit_TaggedValue *arr =
        lhs->kind == MIDLIT_VALUE_ARRAY ? lhs : rhs;
    const struct midlit_TaggedValue *off =
        lhs->kind == MIDLIT_VALUE_ARRAY ? rhs : lhs;

    struct midlit_TaggedValue ptr = midlit_ref_val(&arr->v.arr.elems[0]);

    struct midlit_TaggedValue res =
        eval_arith_ptr_binop(expr, &ptr, off, deinit_queue);

    midgen_dynpush(deinit_queue, ptr);
    return res;
}

static struct midlit_TaggedValue
eval_arith_str_binop(const struct midpar_Expr *expr,
                     const struct midlit_TaggedValue *lhs,
                     const struct midlit_TaggedValue *rhs,
                     struct midlit_TaggedValueVec *deinit_queue)
{
    const struct midlit_TaggedValue *str =
        lhs->kind == MIDLIT_VALUE_STR ? lhs : rhs;
    const struct midlit_TaggedValue *off =
        lhs->kind == MIDLIT_VALUE_STR ? rhs : lhs;

    struct midlit_TaggedValue ptr = midlit_ref_val(&str->v.str.nums[0]);

    struct midlit_TaggedValue res =
        eval_arith_ptr_binop(expr, &ptr, off, deinit_queue);

    midgen_dynpush(deinit_queue, ptr);
    return res;
}

static bool binop_has_ptr_arg(const struct midlit_TaggedValue *lhs,
                              const struct midlit_TaggedValue *rhs)
{
    return lhs->kind == MIDLIT_VALUE_PTR || rhs->kind == MIDLIT_VALUE_PTR;
}

static bool binop_has_arr_arg(const struct midlit_TaggedValue *lhs,
                              const struct midlit_TaggedValue *rhs)
{
    return lhs->kind == MIDLIT_VALUE_ARRAY || rhs->kind == MIDLIT_VALUE_ARRAY;
}

static bool binop_has_str_arg(const struct midlit_TaggedValue *lhs,
                              const struct midlit_TaggedValue *rhs)
{
    return lhs->kind == MIDLIT_VALUE_STR || rhs->kind == MIDLIT_VALUE_STR;
}

static struct midlit_TaggedValue
eval_arith_binop(const struct midpar_Expr *expr, struct midlit_TaggedValue *lhs,
                 struct midlit_TaggedValue *rhs,
                 struct midlit_TaggedValueVec *deinit_queue)
{
    if (binop_has_ptr_arg(lhs, rhs))
        return eval_arith_ptr_binop(expr, lhs, rhs, deinit_queue);
    else if (binop_has_arr_arg(lhs, rhs))
        return eval_arith_arr_binop(expr, lhs, rhs, deinit_queue);
    else if (binop_has_str_arg(lhs, rhs))
        return eval_arith_str_binop(expr, lhs, rhs, deinit_queue);

    enum midlit_ValueKind res_kind = midsema_type_lit_value_kind(&expr->ret);
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
            res.v.flt = midflt_nip_div(&lhs->v.flt, &rhs->v.flt);
        break;

    case MIDPAR_EXPRTYPE_MOD:
        if (res.kind == MIDLIT_VALUE_SIGNED_INT)
            res.v.i = midint_nip_srem(&lhs->v.i, &rhs->v.i);
        else if (res.kind == MIDLIT_VALUE_UNSIGNED_INT)
            res.v.i = midint_nip_urem(&lhs->v.i, &rhs->v.i);
        else
            MID_CRASH("can't compute remainder of floats");
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

static struct midlit_TaggedValue
eval_memb_sel(const struct midpar_Expr *expr,
              const struct midlit_TaggedValue *lhs)
{
    if (expr->info.args.arr[0].ret.spec != MIDPAR_TYPESPEC_CLASS)
        MID_CRASH("fetching fields of unions not yet supported");

    if (lhs->kind == MIDLIT_VALUE_PTR)
        lhs = midlit_deref_ptr(&lhs->v.ptr);

    const char *field_name = expr->info.args.arr[1].info.ident;
    struct midlit_TaggedValue *field =
        midsema_get_structlit_field(&lhs->v.struct_, field_name);
    assert(field);

    return midlit_copy_value(field);
}

static bool should_eval_binop_rhs(const struct midpar_Expr *expr)
{
    return !midsema_is_memb_sel(expr->type);
}

static struct midlit_TaggedValue
eval_binop(const struct midpar_Expr *expr, const struct midsema_Scope *scope,
           struct midlit_TaggedValueVec *deinit_queue)
{
    const struct midpar_Expr *lhs = &expr->info.args.arr[0];
    const struct midpar_Expr *rhs = &expr->info.args.arr[1];

    struct midlit_TaggedValue res_lhs = eval_expr(lhs, scope, deinit_queue);

    bool rhs_evaled = should_eval_binop_rhs(expr);
    struct midlit_TaggedValue res_rhs;
    if (rhs_evaled)
        res_rhs = eval_expr(rhs, scope, deinit_queue);

    struct midlit_TaggedValue res;

    if (midsema_is_arith_op(expr->type))
        res = eval_arith_binop(expr, &res_lhs, &res_rhs, deinit_queue);
    else if (midsema_is_memb_sel(expr->type))
        res = eval_memb_sel(expr, &res_lhs);
    else
        MID_CRASH("constant folding expr type not supported");

    midgen_dynpush(deinit_queue, res_lhs);
    if (rhs_evaled)
        midgen_dynpush(deinit_queue, res_rhs);
    return res;
}

static struct midlit_TaggedValue
eval_expr(const struct midpar_Expr *expr, const struct midsema_Scope *scope,
          struct midlit_TaggedValueVec *deinit_queue)
{
    assert(expr->typechecked);
    assert(expr->constant);

    if (expr->type == MIDPAR_EXPRTYPE_CONST_FOLD)
        return midlit_copy_value(&expr->info.val);

    // TODO: add support for operator overloading
    assert(!expr->overloaded);

    if (midsema_is_unaryop(expr->type))
        return eval_unaryop(expr, scope, deinit_queue);
    else if (midsema_is_binop(expr->type))
        return eval_binop(expr, scope, deinit_queue);
    else
        return eval_leaf(expr, scope);
}

struct midlit_TaggedValue midsema_eval_expr(const struct midpar_Expr *expr,
                                            const struct midsema_Scope *scope)
{
    struct midlit_TaggedValueVec deinit_queue = {};

    struct midlit_TaggedValue res = eval_expr(expr, scope, &deinit_queue);

    midgen_dyndeinit(&deinit_queue, midlit_TaggedValue_deinit);
    return res;
}

static void fold_expr(struct midpar_Expr *expr,
                      const struct midsema_Scope *scope)
{
    struct midlit_TaggedValue val = midsema_eval_expr(expr, scope);

    const struct midlex_Token *tok = expr->tok;
    struct midpar_Type ret = midpar_copy_type(&expr->ret);
    enum midpar_ExprValueType valtype = expr->valtype;

    midpar_Expr_deinit(expr);

    *expr = (struct midpar_Expr){.info.val = val,
                                 .tok = tok,
                                 .ret = ret,
                                 .type = MIDPAR_EXPRTYPE_CONST_FOLD,
                                 .valtype = valtype,
                                 .typechecked = true,
                                 .constant = true};
}

void midsema_const_fold_expr(struct midpar_Expr *expr,
                             const struct midsema_Scope *scope, bool recursive)
{
    assert(expr->typechecked);

    if (expr->constant) {
        fold_expr(expr, scope);
    } else if (recursive && midsema_expr_uses_args(expr->type)) {
        for (int i = 0; i < expr->info.args.len; ++i) {
            struct midpar_Expr *arg = &expr->info.args.arr[i];
            midsema_const_fold_expr(arg, scope, true);
        }
    }
}
