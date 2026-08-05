#include "parser/type.h"
#include "cmd.h"
#include "diag.h"
#include "dynstr.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/find_twin.h"
#include "parser/scope.h"
#include "parser/template.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/template.h"
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

bool midpar_is_typespec_typecheckable(enum midpar_TypeSpec spec)
{
    return spec != MIDPAR_TYPESPEC_TEMPLATED && spec != MIDPAR_TYPESPEC_UNKNOWN;
}

bool midpar_is_typespec_named(enum midpar_TypeSpec spec)
{
    return spec == MIDPAR_TYPESPEC_CLASS || spec == MIDPAR_TYPESPEC_ENUM ||
           spec == MIDPAR_TYPESPEC_UNION || spec == MIDPAR_TYPESPEC_TEMPLATED;
}

enum midpar_TypeSpec midpar_toktype_to_typespec(enum midlex_TokenType type)
{
    switch (type) {
    case MIDLEX_TOKENTYPE_VOID:
        return MIDPAR_TYPESPEC_VOID;

    case MIDLEX_TOKENTYPE_CHAR:
        return MIDPAR_TYPESPEC_CHAR;

    case MIDLEX_TOKENTYPE_WCHAR:
        return MIDPAR_TYPESPEC_WCHAR;

    case MIDLEX_TOKENTYPE_CHAR16:
        return MIDPAR_TYPESPEC_CHAR16;

    case MIDLEX_TOKENTYPE_CHAR32:
        return MIDPAR_TYPESPEC_CHAR32;

    case MIDLEX_TOKENTYPE_INT:
        return MIDPAR_TYPESPEC_INT;

    case MIDLEX_TOKENTYPE_FLOAT:
        return MIDPAR_TYPESPEC_FLOAT;

    case MIDLEX_TOKENTYPE_DOUBLE:
        return MIDPAR_TYPESPEC_DOUBLE;

    case MIDLEX_TOKENTYPE_BOOL:
        return MIDPAR_TYPESPEC_BOOL;

    default:
        MID_CRASH("token is not a type spec");
    }
}

const char *midpar_typespec_to_str(enum midpar_TypeSpec spec)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_VOID:
        return "void";
    case MIDPAR_TYPESPEC_NULLPTR:
        return "nullptr_t";

    case MIDPAR_TYPESPEC_CHAR:
        return "char";
    case MIDPAR_TYPESPEC_SCHAR:
        return "signed char";
    case MIDPAR_TYPESPEC_UCHAR:
        return "unsigned char";

    case MIDPAR_TYPESPEC_SHORT:
        return "short";
    case MIDPAR_TYPESPEC_USHORT:
        return "unsigned short";

    case MIDPAR_TYPESPEC_INT:
        return "int";
    case MIDPAR_TYPESPEC_UINT:
        return "unsigned int";

    case MIDPAR_TYPESPEC_LONG:
        return "long";
    case MIDPAR_TYPESPEC_ULONG:
        return "unsigned long";

    case MIDPAR_TYPESPEC_LONGLONG:
        return "long long";
    case MIDPAR_TYPESPEC_ULONGLONG:
        return "unsigned long long";

    case MIDPAR_TYPESPEC_FLOAT:
        return "float";
    case MIDPAR_TYPESPEC_DOUBLE:
        return "double";
    case MIDPAR_TYPESPEC_LONGDOUBLE:
        return "long double";

    case MIDPAR_TYPESPEC_BOOL:
        return "bool";
    case MIDPAR_TYPESPEC_WCHAR:
        return "wchar_t";
    case MIDPAR_TYPESPEC_CHAR16:
        return "char16_t";
    case MIDPAR_TYPESPEC_CHAR32:
        return "char32_t";

    case MIDPAR_TYPESPEC_AUTO:
        return "auto";

    case MIDPAR_TYPESPEC_CLASS:
        return "class";
    case MIDPAR_TYPESPEC_UNION:
        return "union";
    case MIDPAR_TYPESPEC_ENUM:
        return "enum";

    case MIDPAR_TYPESPEC_INVALID:
    case MIDPAR_TYPESPEC_FUNC:
    case MIDPAR_TYPESPEC_FPTR:
    case MIDPAR_TYPESPEC_ARRAY:
    case MIDPAR_TYPESPEC_TEMPLATED:
    case MIDPAR_TYPESPEC_UNKNOWN:
        printf("spec = %d\n", spec);
        MID_CRASH("can't convert type spec to str");
        return "INVALID-TYPE";
    }
}

bool midpar_is_integral_typespec(enum midpar_TypeSpec spec)
{
    return spec == MIDPAR_TYPESPEC_CHAR || spec == MIDPAR_TYPESPEC_SCHAR ||
           spec == MIDPAR_TYPESPEC_UCHAR || spec == MIDPAR_TYPESPEC_WCHAR ||
           spec == MIDPAR_TYPESPEC_CHAR16 || spec == MIDPAR_TYPESPEC_CHAR32 ||
           spec == MIDPAR_TYPESPEC_SHORT || spec == MIDPAR_TYPESPEC_USHORT ||
           spec == MIDPAR_TYPESPEC_INT || spec == MIDPAR_TYPESPEC_UINT ||
           spec == MIDPAR_TYPESPEC_LONG || spec == MIDPAR_TYPESPEC_ULONG ||
           spec == MIDPAR_TYPESPEC_LONGLONG ||
           spec == MIDPAR_TYPESPEC_ULONGLONG || spec == MIDPAR_TYPESPEC_BOOL;
}

bool midpar_is_signed_integral_typespec(enum midpar_TypeSpec spec)
{
    return (spec == MIDPAR_TYPESPEC_CHAR && midtype_char_signed) ||
           spec == MIDPAR_TYPESPEC_SCHAR ||
           (spec == MIDPAR_TYPESPEC_WCHAR && midtype_wchar_signed) ||
           spec == MIDPAR_TYPESPEC_SHORT || spec == MIDPAR_TYPESPEC_INT ||
           spec == MIDPAR_TYPESPEC_LONG || spec == MIDPAR_TYPESPEC_LONGLONG ||
           spec == MIDPAR_TYPESPEC_BOOL;
}

bool midpar_is_unsigned_integral_typespec(enum midpar_TypeSpec spec)
{
    return midpar_is_integral_typespec(spec) &&
           !midpar_is_signed_integral_typespec(spec);
}

bool midpar_is_floating_typespec(enum midpar_TypeSpec spec)
{
    return spec == MIDPAR_TYPESPEC_FLOAT || spec == MIDPAR_TYPESPEC_DOUBLE ||
           spec == MIDPAR_TYPESPEC_LONGDOUBLE;
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

bool is_ptr_tok(enum midlex_TokenType type)
{
    return type == MIDLEX_TOKENTYPE_MUL;
}

bool is_lv_ref_tok(enum midlex_TokenType type)
{
    return type == MIDLEX_TOKENTYPE_BITWISE_AND;
}

bool is_rv_ref_tok(enum midlex_TokenType type)
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
                       spec_unsignable_err(midpar_typespec_to_str(spec), tok));
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
                       spec_unsignable_err(midpar_typespec_to_str(spec), tok));
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

mid_isize midpar_parse_quals(const struct midlex_Token *toks, mid_isize start,
                             struct midpar_TypeStorQual *squals,
                             struct midpar_TypeDataQual *dquals)
{
    mid_isize i;
    for (i = start; midlex_is_typequal(toks[i].type); ++i) {
        if (midlex_is_typestorqual(toks[i].type))
            midpar_set_squal_flag(squals, toks[i].type);
        else
            midpar_set_dqual_flag(dquals, toks[i].type);
    }

    return i;
}

static struct midpar_Type type_name_type(const struct midlex_Token *toks,
                                         mid_isize start, mid_isize *out_end,
                                         struct midsema_Scope *scope,
                                         struct midpar_Allocators *allocs,
                                         struct mid_DiagVec *diags)
{
    assert(toks[start].type == MIDLEX_TOKENTYPE_IDENTIFIER);

    auto ident = midsema_find_ident_const(scope, toks[start].ident);
    if (!midsema_ident_is_tmplt(ident->type)) {
        if (out_end)
            *out_end = start + 1;
        return midsema_type_name_type(scope, toks[start].ident);
    }

    // the type is a template and therefore we need to parse the template
    // arguments. example:
    //  Type<...>
    //  ^
    // toks[start]
    mid_isize l_angle = start + 1;
    mid_isize r_angle;
    struct midpar_TmpltArgVec args =
        midpar_parse_tmplt_args(toks, l_angle, &r_angle, scope, allocs, diags);
    if (out_end)
        *out_end = r_angle + 1;

    printf("n args = %" MID_PRIisz "\n", args.len);
    auto tmplt = ident->decl->parent;
    struct midpar_Type ret =
        midsema_instantiate_class_tmplt(tmplt, &args, allocs);

    midgen_dyndeinit(&args, midpar_TmpltArg_deinit);
    return ret;
}

// parses the type specifier and its preceding qualifiers
// static const int *const &x
// ^^^^^^^^^^^^^^^^
struct midpar_Type midpar_parse_base(const struct midlex_Token *toks,
                                     mid_isize start, mid_isize *out_end,
                                     struct midsema_Scope *scope,
                                     struct midpar_Allocators *allocs,
                                     struct mid_DiagVec *diags)
{
    struct midpar_Type ret = {};

    mid_isize i = start;

    // this could definitely be written way better

    // point to the token holding the modifier
    const struct midlex_Token *is_signed = NULL;
    const struct midlex_Token *is_unsigned = NULL;
    const struct midlex_Token *is_short = NULL;
    const struct midlex_Token *is_long = NULL;
    const struct midlex_Token *is_longlong = NULL;

    struct midpar_TypeDataQual dquals = {};
    struct midpar_TypeStorQual squals = {};

    bool spec_is_typedef = false;
    bool missing_spec = true;

    for (;
         midlex_is_typequal(toks[i].type) || midlex_is_typemod(toks[i].type) ||
         tok_is_type_spec(scope, &toks[i]) ||
         tok_is_namespace_name(scope, &toks[i]) ||
         toks[i].type == MIDLEX_TOKENTYPE_SCOPE_RES;
         ++i) {
        if (midlex_is_typedataqual(toks[i].type)) {
            midpar_set_dqual_flag(&dquals, toks[i].type);
        } else if (toks[i].type == MIDLEX_TOKENTYPE_SIGNED) {
            if (is_signed)
                midgen_dynpush(diags,
                               unnecessary_qual_warn("signed", &toks[i]));
            if (is_unsigned)
                midgen_dynpush(diags, bad_qual_err("signed", &toks[i]));
            is_signed = &toks[i];
        } else if (toks[i].type == MIDLEX_TOKENTYPE_UNSIGNED) {
            if (is_unsigned)
                midgen_dynpush(diags,
                               unnecessary_qual_warn("unsigned", &toks[i]));
            if (is_signed)
                midgen_dynpush(diags, bad_qual_err("unsigned", &toks[i]));
            is_unsigned = &toks[i];
        } else if (toks[i].type == MIDLEX_TOKENTYPE_SHORT) {
            if (is_short)
                midgen_dynpush(diags, unnecessary_qual_warn("short", &toks[i]));
            else if (is_long || is_longlong)
                midgen_dynpush(diags, bad_qual_err("short", &toks[i]));
            is_short = &toks[i];
        } else if (toks[i].type == MIDLEX_TOKENTYPE_LONG) {
            if (is_longlong || is_short) {
                midgen_dynpush(diags, bad_qual_err("long", &toks[i]));
            } else if (is_long) {
                is_longlong = &toks[i];
                is_long = NULL;
            } else {
                is_long = &toks[i];
            }
        } else if (midlex_is_typequal(toks[i].type)) {
            midpar_set_squal_flag(&squals, toks[i].type);
        } else if (missing_spec) {
            missing_spec = false;
            auto res = midpar_parse_scope_res(toks, i, &i, scope, diags);
            if (toks[i].type == MIDLEX_TOKENTYPE_IDENTIFIER) {
                ret = type_name_type(toks, i, &i, res, allocs, diags);
                --i;
            } else {
                ret = midpar_toktype_to_type(toks[i].type);
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
        struct mid_Diag err = {.pos = toks[start].pos,
                               .line = toks[start].line,
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
    const struct midlex_Token *toks, mid_isize start, mid_isize min,
    mid_isize *out_end, struct midsema_Scope *scope,
    const struct midpar_TypeStorQual *squals, struct midpar_Allocators *allocs,
    struct mid_DiagVec *diags);

// returns the end of the function ptr
// void (*func_ptr)(int, float)
//      ^         ^           ^
//    lparen    rparen      return
static mid_isize parse_fptr(struct midpar_Type *type,
                            const struct midlex_Token *toks, mid_isize lparen,
                            mid_isize rparen, mid_isize min,
                            struct midsema_Scope *scope,
                            struct midpar_Allocators *allocs,
                            struct mid_DiagVec *diags)
{
    mid_isize p_lparen = rparen + 1;
    mid_isize p_rparen = midpar_find_twin_paren(toks, p_lparen, MID_ISIZE_MAX);

    type->spec = MIDPAR_TYPESPEC_FPTR;
    type->fptr = mid_malloc(sizeof(*type->fptr));
    type->fptr->ret =
        parse_recursive_part(toks, lparen - 1, min, NULL, scope,
                             &(struct midpar_TypeStorQual){}, allocs, diags);
    type->fptr->params = (struct midpar_TypeVec){};

    mid_isize i = p_lparen + 1;
    while (i < p_rparen) {
        midgen_dynpush(
            &type->fptr->params,
            midpar_parse_type(toks, i, &i, scope, NULL, false, allocs, diags));

        if (toks[i].type != MIDLEX_TOKENTYPE_COMMA &&
            toks[i].type != MIDLEX_TOKENTYPE_R_PAREN) {
            midgen_dynpush(diags, expected_paren(false, toks));
        }

        ++i;
    }

    return p_rparen + 1;
}

// start is the idx of the left bracket
// returns the end of the array
// int arr[height][width]
//        ^             ^
//      start          end
static mid_isize parse_array(struct midpar_Type *type,
                             const struct midlex_Token *toks, mid_isize lparen,
                             mid_isize rparen, mid_isize min,
                             struct midsema_Scope *scope,
                             struct midpar_Allocators *allocs,
                             struct mid_DiagVec *diags)
{
    // TODO: implement this
    MID_CRASH("parse_array not implemented yet");
    (void)type;
    (void)toks;
    (void)lparen;
    (void)rparen;
    (void)min;
    (void)scope;
    (void)allocs;
    (void)diags;
}

static struct midpar_Type parse_recursive_part(
    const struct midlex_Token *toks, mid_isize start, mid_isize min,
    mid_isize *out_end, struct midsema_Scope *scope,
    const struct midpar_TypeStorQual *squals, struct midpar_Allocators *allocs,
    struct mid_DiagVec *diags)
{
    struct midpar_Type ret = {.squals = *squals};

    struct midpar_TypeDataQual dquals = {};

    mid_isize i;
    for (i = start;
         i >= min &&
         (midlex_is_typedataqual(toks[i].type) || is_ptr_tok(toks[i].type) ||
          is_lv_ref_tok(toks[i].type) || is_rv_ref_tok(toks[i].type));
         --i) {
        if (midlex_is_typedataqual(toks[i].type)) {
            midpar_set_dqual_flag(&dquals, toks[i].type);
        } else if (is_ptr_tok(toks[i].type)) {
            midgen_dynpush(&ret.dquals, dquals);
            dquals = (struct midpar_TypeDataQual){};
        } else if (is_lv_ref_tok(toks[i].type)) {
            if (ret.lv_ref || ret.rv_ref)
                midgen_dynpush(diags, type_alr_const_err(&toks[i]));
            else if (ret.dquals.len > 0)
                midgen_dynpush(diags, ptr_to_ref_err(&toks[i]));
            else if (dquals.is_const)
                midgen_dynpush(diags, missplaced_const_err(&toks[i]));
            else
                ret.lv_ref = true;
        } else {
            if (ret.lv_ref || ret.rv_ref)
                midgen_dynpush(diags, type_alr_const_err(&toks[i]));
            else if (ret.dquals.len > 0)
                midgen_dynpush(diags, ptr_to_ref_err(&toks[i]));
            else if (dquals.is_const)
                midgen_dynpush(diags, missplaced_const_err(&toks[i]));
            else
                ret.rv_ref = true;
        }
    }

    // end is non inclusive
    mid_isize end = start + 1;

    if (toks[i].type == MIDLEX_TOKENTYPE_L_PAREN) {
        mid_isize rparen = midpar_find_twin_paren(toks, i, MID_ISIZE_MAX);
        if (rparen == -1) {
            midgen_dynpush(diags, expected_paren(false, &toks[i]));
        } else {
            if (toks[rparen + 1].type == MIDLEX_TOKENTYPE_L_PAREN)
                end = parse_fptr(&ret, toks, i, rparen, min, scope, allocs,
                                 diags);
            else if (toks[rparen + 1].type == MIDLEX_TOKENTYPE_L_SQBRACKET)
                end = parse_array(&ret, toks, i, rparen, min, scope, allocs,
                                  diags);
        }
    } else if (toks[end].type == MIDLEX_TOKENTYPE_IDENTIFIER) {
        ++end;
    }

    if (out_end)
        *out_end = end;

    return ret;
}

// starts right after the type specifier
// int const *((*const x)(int))
//           ^
//         start
mid_isize find_type_center(const struct midlex_Token *toks, mid_isize start)
{
    mid_isize i = start;
    while (toks[i].type == MIDLEX_TOKENTYPE_L_PAREN ||
           midlex_is_typedataqual(toks[i].type) || is_ptr_tok(toks[i].type) ||
           is_lv_ref_tok(toks[i].type) || is_rv_ref_tok(toks[i].type))
        ++i;

    if (toks[i].type == MIDLEX_TOKENTYPE_IDENTIFIER)
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
                     const struct midlex_Token *type_start,
                     struct mid_DiagVec *diags)
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
        } else if (midpar_is_typespec_named(base->spec)) {
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

struct midpar_Type midpar_parse_type(const struct midlex_Token *toks,
                                     mid_isize start, mid_isize *out_end,
                                     struct midsema_Scope *scope,
                                     mid_isize *out_declname, bool is_type_id,
                                     struct midpar_Allocators *allocs,
                                     struct mid_DiagVec *diags)
{
    mid_isize i;
    auto base = midpar_parse_base(toks, start, &i, scope, allocs, diags);

    auto ret =
        midpar_parse_type_no_base(toks, i, out_end, &base, scope, out_declname,
                                  is_type_id, allocs, diags);

    midpar_Type_deinit(&base);
    return ret;
}

struct midpar_Type
midpar_parse_type_no_base(const struct midlex_Token *toks, mid_isize start,
                          mid_isize *out_end, const struct midpar_Type *base,
                          struct midsema_Scope *scope, mid_isize *out_declname,
                          bool is_type_id, struct midpar_Allocators *allocs,
                          struct mid_DiagVec *diags)
{
    mid_isize c = find_type_center(toks, start);

    bool has_declname = toks[c].type == MIDLEX_TOKENTYPE_IDENTIFIER &&
                        !midsema_is_type_name(scope, toks[c].ident);
    if (has_declname && is_type_id)
        midgen_dynpush(
            diags, middiag_type_id_w_name_err(&toks[c], MIDDIAG_ERR_BAD_TYPE));

    auto ret = parse_recursive_part(toks, c - has_declname, start, out_end,
                                    scope, &base->squals, allocs, diags);
    add_base(&ret, base, &toks[start], diags);

    if (out_declname)
        *out_declname = has_declname ? c : -1;
    return ret;
}

mid_isize midpar_n_indir(const struct midpar_Type *type)
{
    return type->dquals.len - 1;
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
    } else if (midpar_is_typespec_named(type->spec)) {
        ret.named = type->named;
    } else if (type->spec == MIDPAR_TYPESPEC_FUNC) {
        ret.func = type->func;
    }

    return ret;
}

struct midpar_Type midpar_ref_type(const struct midpar_Type *type,
                                   bool *out_failed)
{
    auto ret = midpar_copy_type(type);

    if (!ret.lv_ref && !ret.rv_ref) {
        midgen_dynpush(&ret.dquals, (struct midpar_TypeDataQual){});
        if (out_failed)
            *out_failed = false;
    } else if (out_failed) {
        *out_failed = true;
    }

    return ret;
}

struct midpar_Type midpar_deref_type(const struct midpar_Type *type,
                                     bool *out_failed)
{
    auto ret = midpar_copy_type(type);

    if (ret.dquals.len > 1) {
        // the first element holds the top most ptr
        midgen_dynremove(&ret.dquals, 0);
        if (out_failed)
            *out_failed = false;
    } else if (out_failed) {
        *out_failed = true;
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
        ret.spec = MIDPAR_TYPESPEC_CLASS;
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

static void type_to_str_impl(const struct midpar_Type *type,
                             struct mid_Dynstr *str);

static void fptr_to_str(const struct midpar_Type *type, struct mid_Dynstr *str)
{
    type_to_str_impl(&type->fptr->ret, str);
    midstr_append_char(str, ' ');

    midstr_append_char(str, '(');
    for (mid_isize i = 0; i < midpar_n_indir(type) + 1; ++i)
        midstr_append_char(str, '*');
    if (type->lv_ref)
        midstr_append_char(str, '&');
    else if (type->rv_ref)
        midstr_append(str, "&&");
    midstr_append_char(str, ')');

    midstr_append_char(str, '(');
    for (mid_isize i = 0; i < type->fptr->params.len; ++i) {
        if (i > 0)
            midstr_append(str, ", ");
        type_to_str_impl(&type->fptr->params.arr[i], str);
    }
    midstr_append_char(str, ')');
}

static void array_to_str(const struct midpar_Type *type, struct mid_Dynstr *str)
{
    type_to_str_impl(&type->array->elem, str);
    midstr_append_printf(str, "[%" PRIu64 "]", type->array->len);
}

static void dquals_to_str(const struct midpar_TypeDataQual *dquals,
                          struct mid_Dynstr *str, bool leading_space,
                          bool trailing_space)
{
    if (dquals->is_const) {
        if (leading_space)
            midstr_append_char(str, ' ');
        midstr_append(str, "const");
        if (trailing_space)
            midstr_append_char(str, ' ');
    }
}

static void regular_type_to_str(const struct midpar_Type *type,
                                struct mid_Dynstr *str)
{
    dquals_to_str(&type->dquals.arr[type->dquals.len - 1], str, false, true);
    midstr_append(str, midpar_typespec_to_str(type->spec));
    if (midpar_is_typespec_named(type->spec))
        midstr_append_printf(
            str, " %s", type->named.parent->idents.arr[type->named.idx].name);

    for (mid_isize i = midpar_n_indir(type); i > 0; --i) {
        midstr_append_char(str, '*');
        dquals_to_str(&type->dquals.arr[i - 1], str, true, false);
    }

    if (type->lv_ref)
        midstr_append_char(str, '&');
    else if (type->rv_ref)
        midstr_append(str, "&&");
}

static void type_to_str_impl(const struct midpar_Type *type,
                             struct mid_Dynstr *str)
{
    if (type->spec == MIDPAR_TYPESPEC_FPTR)
        fptr_to_str(type, str);
    else if (type->spec == MIDPAR_TYPESPEC_ARRAY)
        array_to_str(type, str);
    else if (type->spec == MIDPAR_TYPESPEC_INVALID)
        midstr_append(str, "INVALID-TYPE");
    else
        regular_type_to_str(type, str);
}

char *midpar_type_to_str(const struct midpar_Type *type)
{
    struct mid_Dynstr str = midstr_init();
    type_to_str_impl(type, &str);
    return str.str;
}

bool midpar_valid_type_start(const struct midlex_Token *toks, mid_isize idx,
                             const struct midsema_Scope *scope)
{
    if (midlex_is_typemod(toks[idx].type) || midlex_is_typequal(toks[idx].type))
        return true;

    struct mid_DiagVec tmp = {};
    mid_isize res_end;
    auto res = midpar_parse_scope_res_const(toks, idx, &res_end, scope, &tmp);
    midgen_dyndeinit(&tmp);

    return tok_is_type_spec(res, &toks[res_end]);
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

/*
 Every integer type has an integer conversion rank defined as follows:

— No two signed integer types other than char and signed char (if char is
signed) shall have the same rank, even if they have the same representation.

— The rank of a signed integer type shall be greater than the rank of any signed
integer type with a smaller size.

— The rank of long long int shall be greater than the rank of long int, which
shall be greater than the rank of int, which shall be greater than the rank of
short int, which shall be greater than the rank of signed char.

— The rank of any unsigned integer type shall equal the rank of the
corresponding signed integer type

— The rank of any standard integer type shall be greater than the rank of any
extended integer type with the same size.

— The rank of char shall equal the rank of signed char and unsigned char.

— The rank of bool shall be less than the rank of all other standard integer
types.

— The ranks of char16_t, char32_t, and wchar_t shall equal the ranks of their
underlying types (3.9.1).

— The rank of any extended signed integer type relative to another extended
signed integer type with the same size is implementation-defined, but still
subject to the other rules for determining the integer conversion rank.

— For all integer types T1, T2, and T3, if T1 has greater rank than T2 and T2
has greater rank than T3, then T1 shall have greater rank than T3.
 */

int32_t midpar_typespec_conv_rank(enum midpar_TypeSpec spec)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_BOOL:
        return 10;

    case MIDPAR_TYPESPEC_CHAR:
    case MIDPAR_TYPESPEC_SCHAR:
    case MIDPAR_TYPESPEC_UCHAR:
        return 20;

    case MIDPAR_TYPESPEC_SHORT:
    case MIDPAR_TYPESPEC_USHORT:
        return 30;

    case MIDPAR_TYPESPEC_INT:
    case MIDPAR_TYPESPEC_UINT:
        return 40;

    case MIDPAR_TYPESPEC_LONG:
    case MIDPAR_TYPESPEC_ULONG:
        return 50;

    case MIDPAR_TYPESPEC_LONGLONG:
    case MIDPAR_TYPESPEC_ULONGLONG:
        return 60;

    case MIDPAR_TYPESPEC_FLOAT:
        return 70;

    case MIDPAR_TYPESPEC_DOUBLE:
        return 80;

    case MIDPAR_TYPESPEC_LONGDOUBLE:
        return 90;

    case MIDPAR_TYPESPEC_WCHAR:
        if (midtype_wchar_signed)
            return midpar_typespec_conv_rank(
                midpar_sint_type_of_width(midtype_wchar_size));
        else
            return midpar_typespec_conv_rank(
                midpar_uint_type_of_width(midtype_wchar_size));
    case MIDPAR_TYPESPEC_CHAR16:
        return midpar_typespec_conv_rank(midpar_uint_type_of_width(16 / 8));
    case MIDPAR_TYPESPEC_CHAR32:
        return midpar_typespec_conv_rank(midpar_uint_type_of_width(32 / 8));

    default:
        MID_CRASH("type doesn't have a rank");
    }
}

uint64_t midpar_integral_max(enum midpar_TypeSpec spec)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_CHAR:
        return midtype_char_signed ? midtype_char_smax : midtype_char_umax;
    case MIDPAR_TYPESPEC_SCHAR:
        return midtype_char_smax;
    case MIDPAR_TYPESPEC_UCHAR:
        return midtype_char_umax;
    case MIDPAR_TYPESPEC_WCHAR:
        if (midtype_wchar_signed)
            return midpar_integral_max(
                midpar_sint_type_of_width(midtype_wchar_size));
        else
            return midpar_integral_max(
                midpar_uint_type_of_width(midtype_wchar_size));
    case MIDPAR_TYPESPEC_CHAR16:
        return midpar_integral_max(midpar_uint_type_of_width(16 / 8));
    case MIDPAR_TYPESPEC_CHAR32:
        return midpar_integral_max(midpar_uint_type_of_width(32 / 8));

    case MIDPAR_TYPESPEC_SHORT:
        return midtype_short_smax;
    case MIDPAR_TYPESPEC_USHORT:
        return midtype_short_umax;

    case MIDPAR_TYPESPEC_INT:
        return midtype_int_smax;
    case MIDPAR_TYPESPEC_UINT:
        return midtype_int_umax;

    case MIDPAR_TYPESPEC_LONG:
        return midtype_long_smax;
    case MIDPAR_TYPESPEC_ULONG:
        return midtype_long_umax;

    case MIDPAR_TYPESPEC_LONGLONG:
        return midtype_longlong_smax;
    case MIDPAR_TYPESPEC_ULONGLONG:
        return midtype_longlong_umax;

    case MIDPAR_TYPESPEC_BOOL:
        return 1;

    default:
        assert(!midpar_is_integral_typespec(spec));
        MID_CRASH("spec isn't integral");
    }
}

int64_t midpar_integral_min(enum midpar_TypeSpec spec)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_CHAR:
        return midtype_char_signed ? midtype_char_smin : 0;
    case MIDPAR_TYPESPEC_SCHAR:
        return midtype_char_smin;
    case MIDPAR_TYPESPEC_UCHAR:
        return 0;
    case MIDPAR_TYPESPEC_WCHAR:
        if (midtype_wchar_signed)
            return midpar_integral_min(
                midpar_sint_type_of_width(midtype_wchar_size));
        else
            return 0;
    case MIDPAR_TYPESPEC_CHAR16:
        return 0;
    case MIDPAR_TYPESPEC_CHAR32:
        return 0;

    case MIDPAR_TYPESPEC_SHORT:
        return midtype_short_smin;
    case MIDPAR_TYPESPEC_USHORT:
        return 0;

    case MIDPAR_TYPESPEC_INT:
        return midtype_int_smin;
    case MIDPAR_TYPESPEC_UINT:
        return 0;

    case MIDPAR_TYPESPEC_LONG:
        return midtype_long_smin;
    case MIDPAR_TYPESPEC_ULONG:
        return 0;

    case MIDPAR_TYPESPEC_LONGLONG:
        return midtype_longlong_smin;
    case MIDPAR_TYPESPEC_ULONGLONG:
        return 0;

    case MIDPAR_TYPESPEC_BOOL:
        return 0;

    default:
        assert(!midpar_is_integral_typespec(spec));
        MID_CRASH("spec isn't integral");
    }
}

enum midpar_TypeSpec midpar_integral_prom(enum midpar_TypeSpec spec)
{
    assert(midpar_is_integral_typespec(spec));

    if (spec == MIDPAR_TYPESPEC_BOOL)
        return MIDPAR_TYPESPEC_INT;

    int32_t spec_rank = midpar_typespec_conv_rank(spec);
    int32_t int_rank = midpar_typespec_conv_rank(MIDPAR_TYPESPEC_INT);

    if (spec_rank < int_rank) {
        if (midpar_integral_max(MIDPAR_TYPESPEC_INT) >=
                midpar_integral_max(spec) &&
            midpar_integral_min(MIDPAR_TYPESPEC_INT) <=
                midpar_integral_min(spec))
            return MIDPAR_TYPESPEC_INT;
        else
            return MIDPAR_TYPESPEC_UINT;
    } else {
        return spec;
    }
}

bool midpar_is_fundamental_type(const struct midpar_Type *type)
{
    return midpar_n_indir(type) == 0 &&
           (midpar_is_integral_typespec(type->spec) ||
            midpar_is_floating_typespec(type->spec));
}

static bool are_fptrs_same(const struct midpar_TypeFPtr *a,
                           const struct midpar_TypeFPtr *b)
{
    if (a->params.len != b->params.len)
        return false;
    else if (a->has_ellipsis != b->has_ellipsis)
        return false;
    else if (!midpar_are_types_same(&a->ret, &b->ret))
        return false;

    for (mid_isize i = 0; i < a->params.len; ++i) {
        if (!midpar_are_types_same(&a->params.arr[i], &b->params.arr[i]))
            return false;
    }

    return true;
}

static bool are_arrays_same(const struct midpar_TypeArray *a,
                            const struct midpar_TypeArray *b)
{
    if (a->len != b->len)
        return false;

    return midpar_are_types_same(&a->elem, &b->elem);
}

bool midpar_dquals_same(const struct midpar_TypeDataQual *a, mid_isize n_a,
                        const struct midpar_TypeDataQual *b, mid_isize n_b)
{
    if (n_a != n_b)
        return false;

    for (mid_isize i = 0; i < n_a; ++i) {
        if (a[i].is_const != b[i].is_const ||
            a[i].is_volatile != b[i].is_volatile)
            return false;
    }

    return true;
}

bool midpar_squals_same(const struct midpar_TypeStorQual *a,
                        const struct midpar_TypeStorQual *b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

bool midpar_are_types_same(const struct midpar_Type *a,
                           const struct midpar_Type *b)
{
    if (a->spec != b->spec)
        return false;
    else if (a->lv_ref != b->lv_ref || a->rv_ref != b->rv_ref)
        return false;
    else if (!midpar_squals_same(&a->squals, &b->squals))
        return false;
    else if (!midpar_dquals_same(a->dquals.arr, a->dquals.len, b->dquals.arr,
                                 b->dquals.len))
        return false;
    else if (a->spec == MIDPAR_TYPESPEC_FPTR)
        return are_fptrs_same(a->fptr, b->fptr);
    else if (a->spec == MIDPAR_TYPESPEC_ARRAY)
        return are_arrays_same(a->array, b->array);
    else if (midpar_is_typespec_named(a->spec))
        return a->named.parent == b->named.parent &&
               a->named.idx == b->named.idx;
    else
        return true;
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

struct midpar_Type midpar_create_unknown_type()
{
    struct midpar_Type ret = {.spec = MIDPAR_TYPESPEC_UNKNOWN};

    midgen_dynpush(&ret.dquals, ((struct midpar_TypeDataQual){}));

    return ret;
}

bool midpar_type_is_void(const struct midpar_Type *type)
{
    return midpar_n_indir(type) == 0 && type->spec == MIDPAR_TYPESPEC_VOID;
}

bool midpar_type_is_void_ptr(const struct midpar_Type *type)
{
    return midpar_n_indir(type) == 1 && type->spec == MIDPAR_TYPESPEC_VOID;
}

bool midpar_type_is_nullptr_t(const struct midpar_Type *type)
{
    return midpar_n_indir(type) == 0 && type->spec == MIDPAR_TYPESPEC_NULLPTR;
}

bool midpar_type_is_ref(const struct midpar_Type *type)
{
    return type->lv_ref || type->rv_ref;
}

bool midpar_type_is_typecheckable(const struct midpar_Type *type)
{
    return midpar_is_typespec_typecheckable(type->spec);
}
