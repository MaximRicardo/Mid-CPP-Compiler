#include "parser/type.h"
#include "apint.h"
#include "cmd.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "literal.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/ast.h"
#include "parser/end_types.h"
#include "parser/expr.h"
#include "parser/find_twin.h"
#include "parser/scope.h"
#include "parser/template.h"
#include "sema/expr_eval.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/template.h"
#include "sema/type.h"
#include "types.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool tok_is_type_spec(const struct midsema_Scope *scope,
                             const struct midlex_Token *tok)
{
    return midlex_is_typespec(tok->type) ||
           (tok->type == MIDLEX_TOKENTYPE_IDENTIFIER &&
            midsema_is_type_name(scope, tok->ident));
}

static bool tok_is_namespace_name(const struct midsema_Scope *scope,
                                  const struct midlex_Token *tok)
{
    return tok->type == MIDLEX_TOKENTYPE_IDENTIFIER &&
           midsema_is_namespace_name(scope, tok->ident);
}

void midpar_Type_deinit(struct midpar_Type *self)
{
    if (self->spec == MIDPAR_TYPESPEC_FPTR) {
        midgen_dyndeinit(&self->fptr->params, midpar_Type_deinit);
        midpar_Type_deinit(&self->fptr->ret);
        free(self->fptr);
    } else if (self->spec == MIDPAR_TYPESPEC_ARRAY) {
        midpar_Type_deinit(&self->array->elem);
        free(self->array);
    }

    midgen_dyndeinit(&self->dquals);
}

static bool is_ptr_tok(enum midlex_TokenType type)
{
    return type == MIDLEX_TOKENTYPE_MUL;
}

static bool is_lv_ref_tok(enum midlex_TokenType type)
{
    return type == MIDLEX_TOKENTYPE_BITWISE_AND;
}

static bool is_rv_ref_tok(enum midlex_TokenType type)
{
    return type == MIDLEX_TOKENTYPE_LOGICAL_AND;
}

static struct mid_Diag unnecessary_qual_warn(const char *qual,
                                             const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("unnecessary '%s' qualifier", qual),
        .warn = MIDDIAG_WARN_UNNECESSARY_QUALIFIER,
        .type = MIDDIAG_TYPE_WARNING,
    };
}

static struct mid_Diag ptr_to_ref_err(const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("pointer to a reference is not allowed"),
        .err = MIDDIAG_ERR_PTR_TO_REF,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static struct mid_Diag missplaced_const_err(const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("missplaced const specifier"),
        .err = MIDDIAG_ERR_MISPLACED_QUALIFIER,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static struct mid_Diag type_alr_const_err(const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("type is already a reference"),
        .err = MIDDIAG_ERR_TYPE_ALREADY_REF,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static struct mid_Diag expected_paren(bool left, const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("expected '%c'", left ? '(' : ')'),
        .err = MIDDIAG_ERR_MISSING_PAREN,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static struct mid_Diag spec_unsignable_err(const char *type_name,
                                           const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("type '%s' cannot be made signed or unsigned",
                                 type_name),
        .err = MIDDIAG_ERR_TYPE_UNSIGNABLE,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static struct mid_Diag bad_qual_err(const char *type_name,
                                    const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("bad qualifier '%s'", type_name),
        .err = MIDDIAG_ERR_TYPE_UNSIGNABLE,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static enum midpar_TypeSpec make_spec_signed(enum midpar_TypeSpec spec,
                                             const struct midlex_Token *tok,
                                             struct mid_DiagVec *diags)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_CHAR:
    case MIDPAR_TYPESPEC_SCHAR:
    case MIDPAR_TYPESPEC_UCHAR:
        return MIDPAR_TYPESPEC_SCHAR;

    case MIDPAR_TYPESPEC_SHORT:
    case MIDPAR_TYPESPEC_USHORT:
        return MIDPAR_TYPESPEC_SHORT;

    case MIDPAR_TYPESPEC_INT:
    case MIDPAR_TYPESPEC_UINT:
        return MIDPAR_TYPESPEC_INT;

    case MIDPAR_TYPESPEC_LONG:
    case MIDPAR_TYPESPEC_ULONG:
        return MIDPAR_TYPESPEC_LONG;

    case MIDPAR_TYPESPEC_LONGLONG:
    case MIDPAR_TYPESPEC_ULONGLONG:
        return MIDPAR_TYPESPEC_LONGLONG;

    default:
        midgen_dynpush(diags,
                       spec_unsignable_err(midsema_typespec_to_str(spec), tok));
        return spec;
    }
}

static enum midpar_TypeSpec make_spec_unsigned(enum midpar_TypeSpec spec,
                                               const struct midlex_Token *tok,
                                               struct mid_DiagVec *diags)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_CHAR:
    case MIDPAR_TYPESPEC_SCHAR:
    case MIDPAR_TYPESPEC_UCHAR:
        return MIDPAR_TYPESPEC_UCHAR;

    case MIDPAR_TYPESPEC_SHORT:
    case MIDPAR_TYPESPEC_USHORT:
        return MIDPAR_TYPESPEC_USHORT;

    case MIDPAR_TYPESPEC_INT:
    case MIDPAR_TYPESPEC_UINT:
        return MIDPAR_TYPESPEC_UINT;

    case MIDPAR_TYPESPEC_LONG:
    case MIDPAR_TYPESPEC_ULONG:
        return MIDPAR_TYPESPEC_ULONG;

    case MIDPAR_TYPESPEC_LONGLONG:
    case MIDPAR_TYPESPEC_ULONGLONG:
        return MIDPAR_TYPESPEC_ULONGLONG;

    default:
        midgen_dynpush(diags,
                       spec_unsignable_err(midsema_typespec_to_str(spec), tok));
        return spec;
    }
}

static enum midpar_TypeSpec make_spec_short(enum midpar_TypeSpec spec,
                                            const struct midlex_Token *tok,
                                            struct mid_DiagVec *diags)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_INT:
        return MIDPAR_TYPESPEC_SHORT;
    case MIDPAR_TYPESPEC_UINT:
        return MIDPAR_TYPESPEC_USHORT;

    default:
        midgen_dynpush(diags, bad_qual_err("short", tok));
        return spec;
    }
}

static enum midpar_TypeSpec make_spec_long(enum midpar_TypeSpec spec,
                                           const struct midlex_Token *tok,
                                           struct mid_DiagVec *diags)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_INT:
        return MIDPAR_TYPESPEC_LONG;
    case MIDPAR_TYPESPEC_UINT:
        return MIDPAR_TYPESPEC_ULONG;
    case MIDPAR_TYPESPEC_DOUBLE:
        return MIDPAR_TYPESPEC_LONGDOUBLE;

    default:
        midgen_dynpush(diags, bad_qual_err("long", tok));
        return spec;
    }
}

static enum midpar_TypeSpec make_spec_longlong(enum midpar_TypeSpec spec,
                                               const struct midlex_Token *tok,
                                               struct mid_DiagVec *diags)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_INT:
        return MIDPAR_TYPESPEC_LONGLONG;
    case MIDPAR_TYPESPEC_UINT:
        return MIDPAR_TYPESPEC_ULONGLONG;

    default:
        midgen_dynpush(diags, bad_qual_err("long long", tok));
        return spec;
    }
}

void midpar_set_squal_flag(struct midpar_TypeStorQual *qual,
                           enum midlex_TokenType type)
{
    switch (type) {
    case MIDLEX_TOKENTYPE_STATIC:
        qual->is_static = true;
        break;

    case MIDLEX_TOKENTYPE_CONSTEXPR:
        qual->is_constexpr = true;
        break;

    case MIDLEX_TOKENTYPE_TYPEDEF:
        qual->is_typedef = true;
        break;

    default:
        MID_CRASH("token is not a storage qualifier");
    }
}

void midpar_set_dqual_flag(struct midpar_TypeDataQual *qual,
                           enum midlex_TokenType type)
{
    switch (type) {
    case MIDLEX_TOKENTYPE_CONST:
        qual->is_const = true;
        break;

    case MIDLEX_TOKENTYPE_VOLATILE:
        qual->is_volatile = true;
        break;

    default:
        MID_CRASH("token is not a data qualifier");
    }
}

midlex_TokenIter midpar_parse_quals(midlex_TokenIter start,
                                    struct midpar_TypeStorQual *squals,
                                    struct midpar_TypeDataQual *dquals)
{
    midlex_TokenIter i;
    for (i = start; midlex_is_typequal(i->type); ++i) {
        if (midlex_is_typestorqual(i->type))
            midpar_set_squal_flag(squals, i->type);
        else
            midpar_set_dqual_flag(dquals, i->type);
    }

    return i;
}

static struct midpar_Type type_name_type(midlex_TokenIter start,
                                         midlex_TokenIter *out_end,
                                         struct midsema_Scope *scope,
                                         struct midpar_Allocators *allocs,
                                         struct mid_DiagVec *diags)
{
    assert(start->type == MIDLEX_TOKENTYPE_IDENTIFIER);

    auto ident = midsema_find_ident_const(scope, start->ident);
    if (!midsema_ident_is_tmplt(ident->type)) {
        if (out_end)
            *out_end = start + 1;
        return midsema_type_name_type(scope, start->ident);
    }

    // the type is a template and therefore we need to parse the template
    // arguments. example:
    //  Type<...>
    //  ^
    // toks[start]
    auto l_angle = start + 1;
    midlex_TokenIter r_angle;
    struct midpar_TmpltArgVec args =
        midpar_parse_tmplt_args(l_angle, &r_angle, scope, allocs, diags);
    if (out_end)
        *out_end = r_angle + 1;

    auto tmplt = ident->decl->parent;
    struct midpar_Type ret =
        midsema_instantiate_class_tmplt(tmplt, &args, allocs);

    midgen_dyndeinit(&args, midpar_TmpltArg_deinit);
    return ret;
}

// parses the type specifier and its preceding qualifiers
// static const int *const &x
// ^^^^^^^^^^^^^^^^
struct midpar_Type midpar_parse_base(midlex_TokenIter start,
                                     midlex_TokenIter *out_end,
                                     struct midsema_Scope *scope,
                                     struct midpar_Allocators *allocs,
                                     struct mid_DiagVec *diags)
{
    struct midpar_Type ret = {};

    auto i = start;

    // this could definitely be written way better

    // point to the token holding the modifier
    midlex_TokenIter is_signed = NULL;
    midlex_TokenIter is_unsigned = NULL;
    midlex_TokenIter is_short = NULL;
    midlex_TokenIter is_long = NULL;
    midlex_TokenIter is_longlong = NULL;

    struct midpar_TypeDataQual dquals = {};
    struct midpar_TypeStorQual squals = {};

    bool spec_is_typedef = false;
    bool missing_spec = true;

    for (; midlex_is_typequal(i->type) || midlex_is_typemod(i->type) ||
           tok_is_type_spec(scope, i) || tok_is_namespace_name(scope, i) ||
           i->type == MIDLEX_TOKENTYPE_SCOPE_RES;
         ++i) {
        if (midlex_is_typedataqual(i->type)) {
            midpar_set_dqual_flag(&dquals, i->type);
        } else if (i->type == MIDLEX_TOKENTYPE_SIGNED) {
            if (is_signed)
                midgen_dynpush(diags, unnecessary_qual_warn("signed", i));
            if (is_unsigned)
                midgen_dynpush(diags, bad_qual_err("signed", i));
            is_signed = i;
        } else if (i->type == MIDLEX_TOKENTYPE_UNSIGNED) {
            if (is_unsigned)
                midgen_dynpush(diags, unnecessary_qual_warn("unsigned", i));
            if (is_signed)
                midgen_dynpush(diags, bad_qual_err("unsigned", i));
            is_unsigned = i;
        } else if (i->type == MIDLEX_TOKENTYPE_SHORT) {
            if (is_short)
                midgen_dynpush(diags, unnecessary_qual_warn("short", i));
            else if (is_long || is_longlong)
                midgen_dynpush(diags, bad_qual_err("short", i));
            is_short = i;
        } else if (i->type == MIDLEX_TOKENTYPE_LONG) {
            if (is_longlong || is_short) {
                midgen_dynpush(diags, bad_qual_err("long", i));
            } else if (is_long) {
                is_longlong = i;
                is_long = NULL;
            } else {
                is_long = i;
            }
        } else if (midlex_is_typequal(i->type)) {
            midpar_set_squal_flag(&squals, i->type);
        } else if (missing_spec) {
            missing_spec = false;
            auto res = midpar_parse_scope_res(i, &i, scope, diags);
            if (i->type == MIDLEX_TOKENTYPE_IDENTIFIER) {
                ret = type_name_type(i, &i, res, allocs, diags);
                --i;
            } else {
                ret = midpar_toktype_to_type(i->type);
            }
            spec_is_typedef = ret.squals.is_typedef;
        } else {
            break;
        }
    }
    ret.squals = squals;
    if (!spec_is_typedef && !missing_spec)
        ret.dquals.arr[0] = dquals;
    else if (!spec_is_typedef && missing_spec)
        midgen_dynpush(&ret.dquals, dquals);

    // short, long and long long don't need a type spec
    if ((is_short || is_long || is_longlong) && missing_spec) {
        missing_spec = false;
        ret.spec = MIDPAR_TYPESPEC_INT;
    } else if (missing_spec) {
        struct mid_Diag err = {.pos = start->pos,
                               .line = start->line,
                               .msg = strdup("expected a type specifier"),
                               .err = MIDDIAG_ERR_MISSING_TYPESPEC,
                               .type = MIDDIAG_TYPE_ERROR};
        midgen_dynpush(diags, err);
        ret.spec = MIDPAR_TYPESPEC_INT; // default to int
    }

    if (is_signed)
        ret.spec = make_spec_signed(ret.spec, is_signed, diags);
    else if (is_unsigned)
        ret.spec = make_spec_unsigned(ret.spec, is_unsigned, diags);

    if (is_short)
        ret.spec = make_spec_short(ret.spec, is_short, diags);
    else if (is_long)
        ret.spec = make_spec_long(ret.spec, is_long, diags);
    else if (is_longlong)
        ret.spec = make_spec_longlong(ret.spec, is_longlong, diags);

    if (out_end)
        *out_end = i;
    return ret;
}

static struct midpar_Type parse_recursive_part(
    midlex_TokenIter start, midlex_TokenIter min, midlex_TokenIter *out_end,
    bool ignore_arr_subscr, struct midsema_Scope *scope,
    const struct midpar_TypeStorQual *squals, struct midpar_Allocators *allocs,
    struct mid_DiagVec *diags);

// returns the next token after the end of the function ptr
// void (*func_ptr)(int, float)
//      ^         ^           ^
//    lparen    rparen      end
static midlex_TokenIter
parse_fptr(struct midpar_Type *type, midlex_TokenIter lparen,
           midlex_TokenIter rparen, midlex_TokenIter min,
           struct midsema_Scope *scope, struct midpar_Allocators *allocs,
           struct mid_DiagVec *diags)
{
    midlex_TokenIter p_lparen = rparen + 1;
    midlex_TokenIter p_rparen = midpar_find_twin_paren(p_lparen, nullptr);
    assert(p_rparen);

    type->spec = MIDPAR_TYPESPEC_FPTR;
    type->fptr = mid_malloc(sizeof(*type->fptr));
    type->fptr->ret =
        parse_recursive_part(lparen - 1, min, NULL, false, scope,
                             &(struct midpar_TypeStorQual){}, allocs, diags);
    type->fptr->params = (struct midpar_TypeVec){};

    midlex_TokenIter i = p_lparen + 1;
    while (i < p_rparen) {
        midgen_dynpush(
            &type->fptr->params,
            midpar_parse_type(i, &i, scope, NULL, false, allocs, diags));

        if (i->type != MIDLEX_TOKENTYPE_COMMA &&
            i->type != MIDLEX_TOKENTYPE_R_PAREN) {
            midgen_dynpush(diags, expected_paren(false, i));
        }

        ++i;
    }

    return p_rparen + 1;
}

static struct mid_Diag nonconstexpr_arr_len_err(const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str(
            "the length of an array must be a constant expression"),
        .err = MIDDIAG_ERR_BAD_ARRAY_LENGTH,
        .type = MIDDIAG_TYPE_ERROR};
}

static struct mid_Diag nonint_arr_len_err(const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg =
            midcmd_fmt_to_str("the length of an array must be an integer type"),
        .err = MIDDIAG_ERR_BAD_ARRAY_LENGTH,
        .type = MIDDIAG_TYPE_ERROR};
}

static struct mid_Diag negative_arr_len_err(const struct midlex_Token *tok)
{
    return (struct mid_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = midcmd_fmt_to_str("the length of an array can't be negative"),
        .err = MIDDIAG_ERR_BAD_ARRAY_LENGTH,
        .type = MIDDIAG_TYPE_ERROR};
}

static void set_array_len(struct midpar_TypeArray *arr,
                          const struct midsema_Scope *scope,
                          struct mid_DiagVec *diags)
{
    assert(arr->len_expr);

    // defaults to zero on error for now
    arr->len = 0;

    if (!midsema_type_is_integral(&arr->len_expr->ret))
        midgen_dynpush(diags, nonint_arr_len_err(arr->len_expr->tok));
    else if (!arr->len_expr->constant)
        midgen_dynpush(diags, nonconstexpr_arr_len_err(arr->len_expr->tok));
    else {
        bool failed;
        struct midlit_TaggedValue val =
            midsema_eval_expr(arr->len_expr, scope, &failed);
        assert(!failed);

        if (val.kind == MIDLIT_VALUE_SIGNED_INT && midint_is_negative(&val.v.i))
            midgen_dynpush(diags, negative_arr_len_err(arr->len_expr->tok));
        else
            arr->len = midint_to_uint(&val.v.i);
    }
}

// start is the idx of the left bracket
// returns the next token after the end of the array
// int arr[height][width]
//     ^  ^             ^
//     |  l_bracket    end
//   start
// dquals       - data qualifiers to be pushed to type. if NULL the default
//                qualifiers are pushed instead.
//                if non-NULL, *dquals gets reset to default as well.
static midlex_TokenIter
parse_array(struct midpar_Type *type, midlex_TokenIter start,
            midlex_TokenIter l_bracket, midlex_TokenIter min,
            struct midpar_TypeDataQual *dquals, struct midsema_Scope *scope,
            struct midpar_Allocators *allocs, struct mid_DiagVec *diags)
{
    // the latest dquals need to be appended else they're lost
    if (dquals) {
        midgen_dynpush(&type->dquals, *dquals);
        *dquals = (struct midpar_TypeDataQual){};
    } else {
        midgen_dynpush(&type->dquals, (struct midpar_TypeDataQual){});
    }

    type->spec = MIDPAR_TYPESPEC_ARRAY;
    type->array = mid_calloc(1, sizeof(*type->array));

    midlex_TokenIter r_bracket = midpar_find_twin_sqbracket(l_bracket, nullptr);
    assert(r_bracket);

    midgen_bumpmalloc(&allocs->expr, &type->array->len_expr);
    *type->array->len_expr = midpar_parse_expr(
        l_bracket + 1, MIDPAR_SUBSCRIPT_ENDTYPES, nullptr, scope, diags);

    set_array_len(type->array, scope, diags);

    // keep recursively creating arrays for multi dimensional arrays
    midlex_TokenIter next = r_bracket + 1;
    if (next->type == MIDLEX_TOKENTYPE_L_SQBRACKET) {
        return parse_array(&type->array->elem, start, next, min, nullptr, scope,
                           allocs, diags);
    } else {
        type->array->elem = parse_recursive_part(
            start - 1, min, NULL, true, scope, &(struct midpar_TypeStorQual){},
            allocs, diags);
        return r_bracket + 1;
    }
}

static void parse_recursive_part_loop(
    midlex_TokenIter start, midlex_TokenIter *out_end, midlex_TokenIter min,
    bool ignore_arr_subscr, struct midpar_Type *res,
    struct midpar_TypeDataQual *dquals, struct midsema_Scope *scope,
    struct midpar_Allocators *allocs, struct mid_DiagVec *diags)
{
    midlex_TokenIter end = start + 1;
    if (end->type == MIDLEX_TOKENTYPE_IDENTIFIER)
        ++end;

    if (end->type == MIDLEX_TOKENTYPE_L_SQBRACKET && !ignore_arr_subscr) {
        end =
            parse_array(res, start + 1, end, min, dquals, scope, allocs, diags);
        if (out_end)
            *out_end = end;
        return;
    }

    midlex_TokenIter i;
    for (i = start;
         i >= min && (midlex_is_typedataqual(i->type) || is_ptr_tok(i->type) ||
                      is_lv_ref_tok(i->type) || is_rv_ref_tok(i->type));
         --i) {
        if (midlex_is_typedataqual(i->type)) {
            midpar_set_dqual_flag(dquals, i->type);
        } else if (is_ptr_tok(i->type)) {
            midgen_dynpush(&res->dquals, *dquals);
            *dquals = (struct midpar_TypeDataQual){};
        } else if (is_lv_ref_tok(i->type)) {
            if (res->lv_ref || res->rv_ref)
                midgen_dynpush(diags, type_alr_const_err(i));
            else if (res->dquals.len > 0)
                midgen_dynpush(diags, ptr_to_ref_err(i));
            else if (dquals->is_const)
                midgen_dynpush(diags, missplaced_const_err(i));
            else
                res->lv_ref = true;
        } else {
            if (res->lv_ref || res->rv_ref)
                midgen_dynpush(diags, type_alr_const_err(i));
            else if (res->dquals.len > 0)
                midgen_dynpush(diags, ptr_to_ref_err(i));
            else if (dquals->is_const)
                midgen_dynpush(diags, missplaced_const_err(i));
            else
                res->rv_ref = true;
        }
    }

    if (i->type == MIDLEX_TOKENTYPE_L_PAREN) {
        auto rparen = midpar_find_twin_paren(i, nullptr);
        if (!rparen) {
            midgen_dynpush(diags, expected_paren(false, i));
        } else {
            if ((rparen + 1)->type == MIDLEX_TOKENTYPE_L_PAREN)
                end = parse_fptr(res, i, rparen, min, scope, allocs, diags);
            else if ((rparen + 1)->type == MIDLEX_TOKENTYPE_L_SQBRACKET)
                end = parse_array(res, i, rparen + 1, min, dquals, scope,
                                  allocs, diags);
            else if (i > min)
                parse_recursive_part_loop(i - 1, &end, min, false, res, dquals,
                                          scope, allocs, diags);
        }
    }

    if (out_end)
        *out_end = end;
}

static struct midpar_Type parse_recursive_part(
    midlex_TokenIter start, midlex_TokenIter min, midlex_TokenIter *out_end,
    bool ignore_arr_subscr, struct midsema_Scope *scope,
    const struct midpar_TypeStorQual *squals, struct midpar_Allocators *allocs,
    struct mid_DiagVec *diags)
{
    struct midpar_Type res = {.squals = *squals};
    struct midpar_TypeDataQual dquals = {};

    parse_recursive_part_loop(start, out_end, min, ignore_arr_subscr, &res,
                              &dquals, scope, allocs, diags);

    /*
    if (end->type == MIDLEX_TOKENTYPE_L_SQBRACKET)
        end = parse_array(&res, i, end, min, &dquals, scope, allocs, diags);
        */

    return res;
}

// starts right after the type specifier
// int const *((*const x)(int))
//           ^
//         start
midlex_TokenIter find_type_center(midlex_TokenIter start)
{
    auto i = start;
    while (i->type == MIDLEX_TOKENTYPE_L_PAREN ||
           midlex_is_typedataqual(i->type) || is_ptr_tok(i->type) ||
           is_lv_ref_tok(i->type) || is_rv_ref_tok(i->type))
        ++i;

    if (i->type == MIDLEX_TOKENTYPE_IDENTIFIER)
        ++i;

    return i - 1;
}

struct midpar_TypeFPtr midpar_copy_fptr_type(const struct midpar_TypeFPtr *fptr)
{
    struct midpar_TypeFPtr ret = {.has_ellipsis = fptr->has_ellipsis};
    ret.ret = midpar_copy_type(&fptr->ret);

    for (mid_isize i = 0; i < fptr->params.len; ++i)
        midgen_dynpush(&ret.params, midpar_copy_type(&fptr->params.arr[i]));

    return ret;
}

struct midpar_TypeArray
midpar_copy_array_type(const struct midpar_TypeArray *arr)
{
    struct midpar_TypeArray ret = {};
    ret.elem = midpar_copy_type(&arr->elem);
    ret.len = arr->len;
    return ret;
}

static void add_base(struct midpar_Type *type, const struct midpar_Type *base,
                     midlex_TokenIter type_start, struct mid_DiagVec *diags)
{
    if (type->spec == MIDPAR_TYPESPEC_FPTR) {
        add_base(&type->fptr->ret, base, type_start, diags);
    } else if (type->spec == MIDPAR_TYPESPEC_ARRAY) {
        add_base(&type->array->elem, base, type_start, diags);
    } else {
        if (base->spec == MIDPAR_TYPESPEC_FPTR) {
            type->fptr = mid_malloc(sizeof(*type->fptr));
            *type->fptr = midpar_copy_fptr_type(base->fptr);
        } else if (base->spec == MIDPAR_TYPESPEC_ARRAY) {
            type->array = mid_malloc(sizeof(*type->array));
            *type->array = midpar_copy_array_type(base->array);
        } else if (midsema_is_typespec_named(base->spec)) {
            type->named = base->named;
        } else if (base->spec == MIDPAR_TYPESPEC_FUNC) {
            type->func = base->func;
        }

        if (type->dquals.len > 0 && (base->lv_ref || base->rv_ref))
            midgen_dynpush(diags, ptr_to_ref_err(type_start));

        for (mid_isize i = 0; i < base->dquals.len; ++i)
            midgen_dynpush(&type->dquals, base->dquals.arr[i]);
        type->spec = base->spec;
        type->squals = base->squals;
        type->lv_ref |= base->lv_ref;
        type->rv_ref |= base->rv_ref;
    }
}

struct midpar_Type
midpar_parse_type(midlex_TokenIter start, midlex_TokenIter *out_end,
                  struct midsema_Scope *scope, midlex_TokenIter *out_declname,
                  bool is_type_id, struct midpar_Allocators *allocs,
                  struct mid_DiagVec *diags)
{
    midlex_TokenIter i;
    auto base = midpar_parse_base(start, &i, scope, allocs, diags);

    auto ret = midpar_parse_type_no_base(i, out_end, &base, scope, out_declname,
                                         is_type_id, allocs, diags);

    midpar_Type_deinit(&base);
    return ret;
}

struct midpar_Type midpar_parse_type_no_base(
    midlex_TokenIter start, midlex_TokenIter *out_end,
    const struct midpar_Type *base, struct midsema_Scope *scope,
    midlex_TokenIter *out_declname, bool is_type_id,
    struct midpar_Allocators *allocs, struct mid_DiagVec *diags)
{
    midlex_TokenIter c = find_type_center(start);

    bool has_declname = c->type == MIDLEX_TOKENTYPE_IDENTIFIER &&
                        !midsema_is_type_name(scope, c->ident);
    if (has_declname && is_type_id)
        midgen_dynpush(diags,
                       middiag_type_id_w_name_err(c, MIDDIAG_ERR_BAD_TYPE));

    auto ret = parse_recursive_part(c - has_declname, start, out_end, false,
                                    scope, &base->squals, allocs, diags);
    add_base(&ret, base, start, diags);

    if (out_declname)
        *out_declname = has_declname ? c : nullptr;
    return ret;
}

struct midpar_Type midpar_copy_type(const struct midpar_Type *type)
{
    struct midpar_Type ret = {
        .spec = type->spec,
        .squals = type->squals,
        .lv_ref = type->lv_ref,
        .rv_ref = type->rv_ref,
    };

    for (mid_isize i = 0; i < type->dquals.len; ++i)
        midgen_dynpush(&ret.dquals, type->dquals.arr[i]);

    if (type->spec == MIDPAR_TYPESPEC_FPTR) {
        ret.fptr = mid_malloc(sizeof(*ret.fptr));
        *ret.fptr = midpar_copy_fptr_type(type->fptr);
    } else if (type->spec == MIDPAR_TYPESPEC_ARRAY) {
        ret.array = mid_malloc(sizeof(*ret.array));
        *ret.array = midpar_copy_array_type(type->array);
    } else if (midsema_is_typespec_named(type->spec)) {
        ret.named = type->named;
    } else if (type->spec == MIDPAR_TYPESPEC_FUNC) {
        ret.func = type->func;
    }

    return ret;
}

struct midpar_Type midpar_toktype_to_type(enum midlex_TokenType type)
{
    struct midpar_Type ret = {};
    midgen_dynpush(&ret.dquals, (struct midpar_TypeDataQual){});

    switch (type) {
    case MIDLEX_TOKENTYPE_VOID:
        ret.spec = MIDPAR_TYPESPEC_VOID;
        break;

    case MIDLEX_TOKENTYPE_CHAR:
        ret.spec = MIDPAR_TYPESPEC_CHAR;
        break;

    case MIDLEX_TOKENTYPE_WCHAR:
        ret.spec = MIDPAR_TYPESPEC_WCHAR;
        break;

    case MIDLEX_TOKENTYPE_CHAR16:
        ret.spec = MIDPAR_TYPESPEC_CHAR16;
        break;

    case MIDLEX_TOKENTYPE_CHAR32:
        ret.spec = MIDPAR_TYPESPEC_CHAR32;
        break;

    case MIDLEX_TOKENTYPE_INT:
        ret.spec = MIDPAR_TYPESPEC_INT;
        break;

    case MIDLEX_TOKENTYPE_FLOAT:
        ret.spec = MIDPAR_TYPESPEC_FLOAT;
        break;

    case MIDLEX_TOKENTYPE_DOUBLE:
        ret.spec = MIDPAR_TYPESPEC_DOUBLE;
        break;

    case MIDLEX_TOKENTYPE_BOOL:
        ret.spec = MIDPAR_TYPESPEC_BOOL;
        break;

    case MIDLEX_TOKENTYPE_STRUCT:
    case MIDLEX_TOKENTYPE_CLASS:
        ret.spec = MIDPAR_TYPESPEC_STRUCT;
        break;

    case MIDLEX_TOKENTYPE_UNION:
        ret.spec = MIDPAR_TYPESPEC_UNION;
        break;

    case MIDLEX_TOKENTYPE_ENUM:
        ret.spec = MIDPAR_TYPESPEC_ENUM;
        break;

    case MIDLEX_TOKENTYPE_AUTO:
        ret.spec = MIDPAR_TYPESPEC_AUTO;
        break;

    default:
        MID_CRASH("can only convert POD type spec tokens to midpar_Type");
    }

    return ret;
}

bool midpar_valid_type_start(midlex_TokenIter tok,
                             const struct midsema_Scope *scope)
{
    if (midlex_is_typemod(tok->type) || midlex_is_typequal(tok->type))
        return true;

    struct mid_DiagVec tmp = {};
    midlex_TokenIter res_end;
    auto res = midpar_parse_scope_res_const(tok, &res_end, scope, &tmp);
    midgen_dyndeinit(&tmp);

    return tok_is_type_spec(res, res_end);
}

enum midpar_TypeSpec midpar_uint_type_of_width(int32_t bytes)
{
    if (midtype_char_size == bytes)
        return MIDPAR_TYPESPEC_UCHAR;
    else if (midtype_short_size == bytes)
        return MIDPAR_TYPESPEC_USHORT;
    else if (midtype_int_size == bytes)
        return MIDPAR_TYPESPEC_UINT;
    else if (midtype_long_size == bytes)
        return MIDPAR_TYPESPEC_ULONG;
    else if (midtype_longlong_size == bytes)
        return MIDPAR_TYPESPEC_ULONGLONG;
    else
        return MIDPAR_TYPESPEC_INVALID;
}

enum midpar_TypeSpec midpar_sint_type_of_width(int32_t bytes)
{
    if (midtype_char_size == bytes)
        return MIDPAR_TYPESPEC_SCHAR;
    else if (midtype_short_size == bytes)
        return MIDPAR_TYPESPEC_SHORT;
    else if (midtype_int_size == bytes)
        return MIDPAR_TYPESPEC_INT;
    else if (midtype_long_size == bytes)
        return MIDPAR_TYPESPEC_LONG;
    else if (midtype_longlong_size == bytes)
        return MIDPAR_TYPESPEC_LONGLONG;
    else
        return MIDPAR_TYPESPEC_INVALID;
}

struct midpar_Type midpar_create_func_type(struct midsema_Scope *scope,
                                           const char *name)
{
    struct midpar_Type ret = {};
    ret.spec = MIDPAR_TYPESPEC_FUNC;
    ret.func.scope = scope;
    ret.func.name = name;

    midgen_dynpush(&ret.dquals, ((struct midpar_TypeDataQual){}));

    return ret;
}

struct midpar_Type midpar_create_named_type(struct midsema_IdentPtr ident,
                                            enum midpar_TypeSpec spec)
{
    struct midpar_Type ret = {.spec = spec};
    ret.named = ident;

    midgen_dynpush(&ret.dquals, ((struct midpar_TypeDataQual){}));

    return ret;
}

struct midpar_Type midpar_create_templated_type(struct midsema_IdentPtr ident)
{
    return midpar_create_named_type(ident, MIDPAR_TYPESPEC_TEMPLATED);
}

struct midpar_Type midpar_create_simple_type(enum midpar_TypeSpec spec,
                                             int n_indir)
{
    struct midpar_Type ret = {.spec = spec};

    for (int i = 0; i < n_indir + 1; ++i)
        midgen_dynpush(&ret.dquals, ((struct midpar_TypeDataQual){}));

    return ret;
}

struct midpar_Type midpar_create_unknown_type()
{
    return midpar_create_simple_type(MIDPAR_TYPESPEC_UNKNOWN, 0);
}
