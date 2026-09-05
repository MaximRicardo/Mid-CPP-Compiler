#include "sema/expr_eval.h"
#include "apfloat.h"
#include "apint.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "literal.h"
#include "macros.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "parser/expr_type.h"
#include "parser/func_decl.h"
#include "parser/return.h"
#include "parser/type.h"
#include "sema/expr.h"
#include "sema/func.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/type.h"
#include "types.h"
#include <string.h>

struct FuncArgVal {
    struct midlit_TaggedValue val;
    const char *param; // name of the parameter the value is assigned to
};
midgen_dynarray_struct_named(FuncArgValVec, struct FuncArgVal);

// deinit_queue     - a deinit queue is used to extend the life time of
//                    temporaries until the end of the expression.
// f_args           - if the expr is inside of a func call we're evaluating,
//                    then this holds the relevant func arguments; otherwise
//                    this parameter is NULL.
// failed           - must be non-NULL. set to false on failure, unmodified on
// success.
static struct midlit_TaggedValue
eval_expr(const struct midpar_Expr *expr, const struct midsema_Scope *scope,
          struct midlit_TaggedValueVec *deinit_queue,
          const struct FuncArgValVec *f_args, bool *failed);

// returns a ptr to the subscripted element
static struct midlit_TaggedValue
eval_subscr_ref(const struct midlit_TaggedValue *lhs,
                const struct midlit_TaggedValue *rhs, bool *failed)
{
    bool lhs_int = lhs->kind == MIDLIT_VALUE_SIGNED_INT ||
                   lhs->kind == MIDLIT_VALUE_UNSIGNED_INT;
    const struct midlit_TaggedValue *arr = lhs_int ? rhs : lhs;
    const struct midlit_TaggedValue *idx = lhs_int ? lhs : rhs;

    assert(idx->kind == MIDLIT_VALUE_SIGNED_INT ||
           idx->kind == MIDLIT_VALUE_UNSIGNED_INT);
    assert(arr->kind == MIDLIT_VALUE_PTR || arr->kind == MIDLIT_VALUE_ARRAY ||
           arr->kind == MIDLIT_VALUE_STR);

    if (arr->kind == MIDLIT_VALUE_PTR) {
        struct midlit_TaggedValue elem_ptr = midlit_copy_value(arr);
        if (!midlit_inc_ptr(&elem_ptr.v.ptr, midint_to_sint(&idx->v.i))) {
            *failed = true;
            midlit_TaggedValue_deinit(&elem_ptr);
            return (struct midlit_TaggedValue){};
        }

        return elem_ptr;
    } else if (arr->kind == MIDLIT_VALUE_STR) {
        struct midlit_TaggedValue ptr = midlit_ref_val(&arr->v.str.nums[0]);
        return eval_subscr_ref(&ptr, rhs, failed);
    } else {
        struct midlit_TaggedValue ptr = midlit_ref_val(&arr->v.arr.elems[0]);
        return eval_subscr_ref(&ptr, rhs, failed);
    }
}

static struct midlit_TaggedValue
eval_subscr(const struct midlit_TaggedValue *lhs,
            const struct midlit_TaggedValue *rhs,
            struct midlit_TaggedValueVec *deinit_queue, bool *failed)
{
    struct midlit_TaggedValue elem_ptr = eval_subscr_ref(lhs, rhs, failed);
    if (*failed || midlit_ptr_is_null(&elem_ptr.v.ptr) ||
        elem_ptr.v.ptr.past_end) {
        *failed = true;
        return (struct midlit_TaggedValue){};
    }

    midgen_dynpush(deinit_queue, elem_ptr);
    return midlit_copy_value(midlit_deref_ptr(&elem_ptr.v.ptr));
}

static struct midlit_TaggedValue *
get_ident_value_raw(const struct midpar_Expr *expr,
                    const struct midsema_Scope *scope,
                    const struct FuncArgValVec *f_args, bool *failed)
{
    if (f_args) {
        for (int i = 0; i < f_args->len; ++i) {
            if (!strcmp(expr->info.ident, f_args->arr[i].param))
                return &f_args->arr[i].val;
        }
    }

    auto ident = midsema_find_ident_const(scope, expr->info.ident);
    assert(ident);

    const struct midpar_VarDeclInst *inst = &ident->decl->var_inst;
    if (inst->constexpr_val)
        return inst->constexpr_val;

    *failed = true;
    return nullptr;
}

// fetches a copy of the identifier's value
static struct midlit_TaggedValue
get_ident_value(const struct midpar_Expr *expr,
                const struct midsema_Scope *scope,
                const struct FuncArgValVec *f_args, bool *failed)
{
    const struct midlit_TaggedValue *raw =
        get_ident_value_raw(expr, scope, f_args, failed);
    if (*failed)
        return (struct midlit_TaggedValue){};

    return midlit_copy_value(raw);
}

static struct midlit_TaggedValue create_nullptr_val()
{
    return (struct midlit_TaggedValue){.kind = MIDLIT_VALUE_PTR,
                                       .v.ptr = midlit_null_ptr()};
}

static struct midlit_TaggedValue eval_leaf(const struct midpar_Expr *expr,
                                           const struct midsema_Scope *scope,
                                           const struct FuncArgValVec *f_args,
                                           bool *failed)
{
    if (expr->type == MIDPAR_EXPRTYPE_NULLPTR_LIT)
        return create_nullptr_val();
    else if (midsema_is_numlit(expr->type) || midsema_is_strlit(expr->type))
        return midlit_copy_value(&expr->info.val);
    else if (expr->type == MIDPAR_EXPRTYPE_IDENTIFIER)
        return get_ident_value(expr, scope, f_args, failed);

    *failed = true;
    return (struct midlit_TaggedValue){};
}

static struct midlit_TaggedValue
eval_ref_op(const struct midpar_Expr *expr, const struct midsema_Scope *scope,
            struct midlit_TaggedValueVec *deinit_queue,
            const struct FuncArgValVec *f_args, bool *failed)
{
    const struct midpar_Expr *child = &expr->info.args.arr[0];

    if (child->type == MIDPAR_EXPRTYPE_IDENTIFIER) {
        struct midlit_TaggedValue *raw =
            get_ident_value_raw(child, scope, f_args, failed);
        if (*failed)
            return (struct midlit_TaggedValue){};
        return midlit_ref_val(raw);
    } else if (child->type == MIDPAR_EXPRTYPE_ARRAY_SUBSCR) {

        struct midlit_TaggedValue lhs = eval_expr(
            &child->info.args.arr[0], scope, deinit_queue, f_args, failed);
        if (*failed)
            return (struct midlit_TaggedValue){};

        struct midlit_TaggedValue rhs = eval_expr(
            &child->info.args.arr[1], scope, deinit_queue, f_args, failed);
        if (*failed) {
            midlit_TaggedValue_deinit(&lhs);
            return (struct midlit_TaggedValue){};
        }

        struct midlit_TaggedValue res = eval_subscr_ref(&lhs, &rhs, failed);

        midgen_dynpush(deinit_queue, lhs);
        midgen_dynpush(deinit_queue, rhs);
        return res;

    } else {
        MID_CRASH("can't reference child expr");
    }
}

static struct midlit_TaggedValue
eval_unaryop(const struct midpar_Expr *expr, const struct midsema_Scope *scope,
             struct midlit_TaggedValueVec *deinit_queue,
             const struct FuncArgValVec *f_args, bool *failed)
{
    if (expr->type == MIDPAR_EXPRTYPE_REF)
        return eval_ref_op(expr, scope, deinit_queue, f_args, failed);

    const struct midpar_Expr *child = &expr->info.args.arr[0];

    struct midlit_TaggedValue res;
    if (expr->type != MIDPAR_EXPRTYPE_SIZEOF)
        res = eval_expr(child, scope, deinit_queue, f_args, failed);
    if (*failed)
        return (struct midlit_TaggedValue){};

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
        if (!val) {
            *failed = true;
            midlit_TaggedValue_deinit(&res);
            return (struct midlit_TaggedValue){};
        }
        midgen_dynpush(deinit_queue, res);
        res = midlit_copy_value(val);
        break;
    }

    default:
        MID_CRASH("constant folding expr type not supported");
    }

    return res;
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
                     struct midlit_TaggedValueVec *deinit_queue, bool *failed)
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
            *failed = true;
        break;

    case MIDPAR_EXPRTYPE_SUB:
        if (!midlit_dec_ptr(&res.v.ptr, off_val))
            *failed = true;
        break;

    case MIDPAR_EXPRTYPE_ARRAY_SUBSCR: {
        struct midlit_Ptr tmp = midlit_copy_ptr(&ptr->v.ptr);
        if (!midlit_inc_ptr(&tmp, off_val)) {
            *failed = true;
            break;
        }
        res = midlit_copy_value(midlit_deref_ptr(&tmp));
        midgen_dynpush(deinit_queue, ptr_to_val(&tmp));
        break;
    }

    default:
        MID_CRASH("expr type is not a valid binary ptr arithmetic operator");
    }

    if (*failed) {
        midlit_TaggedValue_deinit(&res);
        return (struct midlit_TaggedValue){};
    }

    return res;
}

static struct midlit_TaggedValue
eval_arith_arr_binop(const struct midpar_Expr *expr,
                     const struct midlit_TaggedValue *lhs,
                     const struct midlit_TaggedValue *rhs,
                     struct midlit_TaggedValueVec *deinit_queue, bool *failed)
{
    const struct midlit_TaggedValue *arr =
        lhs->kind == MIDLIT_VALUE_ARRAY ? lhs : rhs;
    const struct midlit_TaggedValue *off =
        lhs->kind == MIDLIT_VALUE_ARRAY ? rhs : lhs;

    struct midlit_TaggedValue ptr = midlit_ref_val(&arr->v.arr.elems[0]);

    struct midlit_TaggedValue res =
        eval_arith_ptr_binop(expr, &ptr, off, deinit_queue, failed);

    midgen_dynpush(deinit_queue, ptr);
    return res;
}

static struct midlit_TaggedValue
eval_arith_str_binop(const struct midpar_Expr *expr,
                     const struct midlit_TaggedValue *lhs,
                     const struct midlit_TaggedValue *rhs,
                     struct midlit_TaggedValueVec *deinit_queue, bool *failed)
{
    const struct midlit_TaggedValue *str =
        lhs->kind == MIDLIT_VALUE_STR ? lhs : rhs;
    const struct midlit_TaggedValue *off =
        lhs->kind == MIDLIT_VALUE_STR ? rhs : lhs;

    struct midlit_TaggedValue ptr = midlit_ref_val(&str->v.str.nums[0]);

    struct midlit_TaggedValue res =
        eval_arith_ptr_binop(expr, &ptr, off, deinit_queue, failed);

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
                 struct midlit_TaggedValueVec *deinit_queue, bool *failed)
{
    if (binop_has_ptr_arg(lhs, rhs))
        return eval_arith_ptr_binop(expr, lhs, rhs, deinit_queue, failed);
    else if (binop_has_arr_arg(lhs, rhs))
        return eval_arith_arr_binop(expr, lhs, rhs, deinit_queue, failed);
    else if (binop_has_str_arg(lhs, rhs))
        return eval_arith_str_binop(expr, lhs, rhs, deinit_queue, failed);

    enum midlit_ValueKind res_kind = midsema_type_lit_value_kind(&expr->ret);
    bool is_integral = res_kind == MIDLIT_VALUE_SIGNED_INT ||
                       res_kind == MIDLIT_VALUE_UNSIGNED_INT;

    if (res_kind == MIDLIT_VALUE_STR)
        MID_CRASH("can't const fold arithmetic operations on strings");

    midlit_convert_value_deinit_queue(lhs, &expr->ret, deinit_queue);
    midlit_convert_value_deinit_queue(rhs, &expr->ret, deinit_queue);

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
           struct midlit_TaggedValueVec *deinit_queue,
           const struct FuncArgValVec *f_args, bool *failed)
{
    const struct midpar_Expr *lhs = &expr->info.args.arr[0];
    const struct midpar_Expr *rhs = &expr->info.args.arr[1];

    struct midlit_TaggedValue res_rhs = {};
    bool rhs_evaled = false;

    struct midlit_TaggedValue res_lhs =
        eval_expr(lhs, scope, deinit_queue, f_args, failed);
    if (*failed)
        goto failure;

    rhs_evaled = should_eval_binop_rhs(expr);
    if (rhs_evaled) {
        res_rhs = eval_expr(rhs, scope, deinit_queue, f_args, failed);
        if (*failed)
            goto failure;
    }

    struct midlit_TaggedValue res;

    if (midsema_is_arith_op(expr->type))
        res = eval_arith_binop(expr, &res_lhs, &res_rhs, deinit_queue, failed);
    else if (midsema_is_memb_sel(expr->type))
        res = eval_memb_sel(expr, &res_lhs);
    else if (expr->type == MIDPAR_EXPRTYPE_ARRAY_SUBSCR)
        res = eval_subscr(&res_lhs, &res_rhs, deinit_queue, failed);
    else
        MID_CRASH("constant folding expr type not supported");

    midgen_dynpush(deinit_queue, res_lhs);
    if (rhs_evaled)
        midgen_dynpush(deinit_queue, res_rhs);
    return res;

failure:
    midlit_TaggedValue_deinit(&res_lhs);
    if (rhs_evaled)
        midlit_TaggedValue_deinit(&res_rhs);

    return (struct midlit_TaggedValue){};
}

static struct FuncArgValVec
get_func_arg_vals(const struct midpar_FuncDecl *func,
                  const struct midpar_Expr *args, int n_args,
                  const struct midsema_Scope *scope,
                  struct midlit_TaggedValueVec *deinit_queue,
                  const struct FuncArgValVec *f_args, bool *failed)
{
    assert(n_args <= func->params.len);

    struct FuncArgValVec arg_vals = {};
    midgen_dynreserve(&arg_vals, func->params.len);

    for (int i = 0; i < n_args; ++i) {
        const struct midpar_VarDeclInst *param =
            func->params.arr[i]->insts.arr[0];

        struct FuncArgVal arg_val;
        arg_val.param = param->name;
        arg_val.val = eval_expr(&args[i], scope, deinit_queue, f_args, failed);
        if (*failed)
            goto failure;

        midlit_convert_value_deinit_queue(&arg_val.val, &param->type,
                                          deinit_queue);

        midgen_dynpush(&arg_vals, arg_val);
    }

    // apply default arguments
    for (int i = n_args; i < func->params.len; ++i) {
        const struct midpar_VarDeclInst *param =
            func->params.arr[i]->insts.arr[0];
        const struct midpar_Expr *def_arg =
            midpar_func_ident(func)->func_info.default_args[i];

        struct FuncArgVal arg_val;
        arg_val.param = param->name;
        arg_val.val = eval_expr(def_arg, scope, deinit_queue, f_args, failed);
        if (*failed)
            goto failure;

        midlit_convert_value_deinit_queue(&arg_val.val, &param->type,
                                          deinit_queue);

        midgen_dynpush(&arg_vals, arg_val);
    }

    return arg_vals;

failure:
    for (mid_isize i = 0; i < arg_vals.len; ++i)
        midgen_dynpush(deinit_queue, arg_vals.arr[i].val);
    midgen_dyndeinit(&arg_vals);
    return (struct FuncArgValVec){};
}

static struct midpar_Return *
find_constexpr_func_ret(const struct midpar_FuncDecl *func)
{
    for (mid_isize i = 0; i < func->nodes.len; ++i) {
        struct midpar_ASTNode *node = func->nodes.arr[i];
        if (node->type == MIDPAR_ASTNODETYPE_RETURN)
            return &node->ret;
    }

    return nullptr;
}

static struct midlit_TaggedValue
eval_regular_func_call(const struct midpar_FuncDecl *func,
                       const struct FuncArgValVec *args,
                       struct midlit_TaggedValueVec *deinit_queue, bool *failed)
{
    const struct midpar_Return *ret_stmt = find_constexpr_func_ret(func);
    if (!ret_stmt) {
        *failed = true;
        return (struct midlit_TaggedValue){};
    }

    if (!ret_stmt->expr)
        return (struct midlit_TaggedValue){};

    struct midlit_TaggedValue val =
        eval_expr(ret_stmt->expr, midpar_func_ident(func)->func_info.def_scope,
                  deinit_queue, args, failed);
    if (*failed)
        return (struct midlit_TaggedValue){};
    midlit_convert_value_deinit_queue(&val, &func->ret, deinit_queue);
    return val;
}

static struct midlit_TaggedValue
eval_func_call(const struct midpar_Expr *call,
               const struct midsema_Scope *scope,
               struct midlit_TaggedValueVec *deinit_queue,
               const struct FuncArgValVec *f_args, bool *failed)
{
    if (!call->node)
        MID_CRASH(
            "evaluating calls to function ptrs hasn't been implemented yet");

    const struct midpar_FuncDecl *func = &call->node->func_decl;
    // make sure we're looking at the func definition
    func = &midpar_func_ident(func)->def->func_decl;
    if (!func) {
        // we can't evaluate calls to undefined functions
        *failed = true;
        return (struct midlit_TaggedValue){};
    }

    struct FuncArgValVec arg_vals = get_func_arg_vals(
        func, &call->info.args.arr[1], call->info.args.len - 1, scope,
        deinit_queue, f_args, failed);
    if (*failed)
        return (struct midlit_TaggedValue){};

    struct midlit_TaggedValue res;
    if (midsema_func_is_ctor(func)) {
        MID_CRASH("evaluating ctor calls hasn't been implemented yet");
    } else {
        res = eval_regular_func_call(func, &arg_vals, deinit_queue, failed);
    }

    for (int i = 0; i < arg_vals.len; ++i)
        midgen_dynpush(deinit_queue, arg_vals.arr[i].val);
    midgen_dyndeinit(&arg_vals);

    return res;
}

static struct midlit_TaggedValue
eval_expr(const struct midpar_Expr *expr, const struct midsema_Scope *scope,
          struct midlit_TaggedValueVec *deinit_queue,
          const struct FuncArgValVec *f_args, bool *failed)
{
    assert(expr->typechecked);

    if (expr->type == MIDPAR_EXPRTYPE_CONST_FOLD)
        return midlit_copy_value(&expr->info.val);

    // TODO: add support for operator overloading
    if (expr->overloaded)
        return (struct midlit_TaggedValue){};

    if (expr->type == MIDPAR_EXPRTYPE_FUNC_CALL)
        return eval_func_call(expr, scope, deinit_queue, f_args, failed);
    else if (midsema_is_unaryop(expr->type))
        return eval_unaryop(expr, scope, deinit_queue, f_args, failed);
    else if (midsema_is_binop(expr->type))
        return eval_binop(expr, scope, deinit_queue, f_args, failed);
    else
        return eval_leaf(expr, scope, f_args, failed);
}

struct midlit_TaggedValue midsema_eval_expr(const struct midpar_Expr *expr,
                                            const struct midsema_Scope *scope,
                                            bool *failed)
{
    struct midlit_TaggedValueVec deinit_queue = {};

    *failed = false;
    struct midlit_TaggedValue res =
        eval_expr(expr, scope, &deinit_queue, nullptr, failed);

    midgen_dyndeinit(&deinit_queue, midlit_TaggedValue_deinit);
    if (*failed)
        return (struct midlit_TaggedValue){};
    return res;
}

struct midlit_TaggedValue
midsema_eval_expr_mut(struct midpar_Expr *expr,
                      const struct midsema_Scope *scope)
{
    bool failed;
    struct midlit_TaggedValue res = midsema_eval_expr(expr, scope, &failed);
    expr->constant = !failed;

    return res;
}

// returns true on success, false on failure
static bool fold_expr(struct midpar_Expr *expr,
                      const struct midsema_Scope *scope)
{
    struct midlit_TaggedValue val = midsema_eval_expr_mut(expr, scope);
    if (!expr->constant)
        return false;

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

    return true;
}

void midsema_const_fold_expr(struct midpar_Expr *expr,
                             const struct midsema_Scope *scope, bool recursive)
{
    assert(expr->typechecked);

    if (fold_expr(expr, scope))
        return;

    if (recursive && midsema_expr_uses_args(expr->type)) {
        for (int i = 0; i < expr->info.args.len; ++i) {
            struct midpar_Expr *arg = &expr->info.args.arr[i];
            midsema_const_fold_expr(arg, scope, true);
        }
    }
}
