#include "type.h"
#include "diag.h"
#include "dynstr.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "lexer/token_type.h"
#include "macros.h"
#include "mid_alloc.h"
#include "parser/find_twin.h"
#include "parser/template.h"
#include "print.h"
#include "scope.h"
#include "sema/ident.h"
#include "sema/scope.h"
#include "sema/template.h"
#include "types.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool tok_is_type_spec(const struct MidSema_Scope *scope,
                             const struct MidLexer_Token *tok)
{
    return MidLexer_is_typespec(tok->type) ||
           (tok->type == MIDLEXER_TOKENTYPE_IDENTIFIER &&
            MidSema_is_type_name(scope, tok->ident));
}

static bool tok_is_namespace_name(const struct MidSema_Scope *scope,
                                  const struct MidLexer_Token *tok)
{
    return tok->type == MIDLEXER_TOKENTYPE_IDENTIFIER &&
           MidSema_is_namespace_name(scope, tok->ident);
}

bool MidParser_is_typespec_typecheckable(enum MidParser_TypeSpec spec)
{
    return spec != MIDPARSER_TYPESPEC_TEMPLATED &&
           spec != MIDPARSER_TYPESPEC_UNKNOWN;
}

bool MidParser_is_typespec_named(enum MidParser_TypeSpec spec)
{
    return spec == MIDPARSER_TYPESPEC_CLASS ||
           spec == MIDPARSER_TYPESPEC_ENUM ||
           spec == MIDPARSER_TYPESPEC_UNION ||
           spec == MIDPARSER_TYPESPEC_TEMPLATED;
}

enum MidParser_TypeSpec
MidParser_toktype_to_typespec(enum MidLexer_TokenType type)
{
    switch (type) {
    case MIDLEXER_TOKENTYPE_VOID:
        return MIDPARSER_TYPESPEC_VOID;

    case MIDLEXER_TOKENTYPE_CHAR:
        return MIDPARSER_TYPESPEC_CHAR;

    case MIDLEXER_TOKENTYPE_WCHAR:
        return MIDPARSER_TYPESPEC_WCHAR;

    case MIDLEXER_TOKENTYPE_CHAR16:
        return MIDPARSER_TYPESPEC_CHAR16;

    case MIDLEXER_TOKENTYPE_CHAR32:
        return MIDPARSER_TYPESPEC_CHAR32;

    case MIDLEXER_TOKENTYPE_INT:
        return MIDPARSER_TYPESPEC_INT;

    case MIDLEXER_TOKENTYPE_FLOAT:
        return MIDPARSER_TYPESPEC_FLOAT;

    case MIDLEXER_TOKENTYPE_DOUBLE:
        return MIDPARSER_TYPESPEC_DOUBLE;

    case MIDLEXER_TOKENTYPE_BOOL:
        return MIDPARSER_TYPESPEC_BOOL;

    default:
        MID_CRASH("token is not a type spec");
    }
}

const char *MidParser_typespec_to_str(enum MidParser_TypeSpec spec)
{
    switch (spec) {
    case MIDPARSER_TYPESPEC_VOID:
        return "void";
    case MIDPARSER_TYPESPEC_NULLPTR:
        return "nullptr_t";

    case MIDPARSER_TYPESPEC_CHAR:
        return "char";
    case MIDPARSER_TYPESPEC_SCHAR:
        return "signed char";
    case MIDPARSER_TYPESPEC_UCHAR:
        return "unsigned char";

    case MIDPARSER_TYPESPEC_SHORT:
        return "short";
    case MIDPARSER_TYPESPEC_USHORT:
        return "unsigned short";

    case MIDPARSER_TYPESPEC_INT:
        return "int";
    case MIDPARSER_TYPESPEC_UINT:
        return "unsigned int";

    case MIDPARSER_TYPESPEC_LONG:
        return "long";
    case MIDPARSER_TYPESPEC_ULONG:
        return "unsigned long";

    case MIDPARSER_TYPESPEC_LONGLONG:
        return "long long";
    case MIDPARSER_TYPESPEC_ULONGLONG:
        return "unsigned long long";

    case MIDPARSER_TYPESPEC_FLOAT:
        return "float";
    case MIDPARSER_TYPESPEC_DOUBLE:
        return "double";
    case MIDPARSER_TYPESPEC_LONGDOUBLE:
        return "long double";

    case MIDPARSER_TYPESPEC_BOOL:
        return "bool";
    case MIDPARSER_TYPESPEC_WCHAR:
        return "wchar_t";
    case MIDPARSER_TYPESPEC_CHAR16:
        return "char16_t";
    case MIDPARSER_TYPESPEC_CHAR32:
        return "char32_t";

    case MIDPARSER_TYPESPEC_AUTO:
        return "auto";

    case MIDPARSER_TYPESPEC_CLASS:
        return "class";
    case MIDPARSER_TYPESPEC_UNION:
        return "union";
    case MIDPARSER_TYPESPEC_ENUM:
        return "enum";

    case MIDPARSER_TYPESPEC_INVALID:
    case MIDPARSER_TYPESPEC_FUNC:
    case MIDPARSER_TYPESPEC_FPTR:
    case MIDPARSER_TYPESPEC_ARRAY:
    case MIDPARSER_TYPESPEC_TEMPLATED:
    case MIDPARSER_TYPESPEC_UNKNOWN:
        printf("spec = %d\n", spec);
        MID_CRASH("can't convert type spec to str");
        return "INVALID-TYPE";
    }
}

bool MidParser_is_integral_typespec(enum MidParser_TypeSpec spec)
{
    return spec == MIDPARSER_TYPESPEC_CHAR ||
           spec == MIDPARSER_TYPESPEC_SCHAR ||
           spec == MIDPARSER_TYPESPEC_UCHAR ||
           spec == MIDPARSER_TYPESPEC_WCHAR ||
           spec == MIDPARSER_TYPESPEC_CHAR16 ||
           spec == MIDPARSER_TYPESPEC_CHAR32 ||
           spec == MIDPARSER_TYPESPEC_SHORT ||
           spec == MIDPARSER_TYPESPEC_USHORT ||
           spec == MIDPARSER_TYPESPEC_INT || spec == MIDPARSER_TYPESPEC_UINT ||
           spec == MIDPARSER_TYPESPEC_LONG ||
           spec == MIDPARSER_TYPESPEC_ULONG ||
           spec == MIDPARSER_TYPESPEC_LONGLONG ||
           spec == MIDPARSER_TYPESPEC_ULONGLONG ||
           spec == MIDPARSER_TYPESPEC_BOOL;
}

bool MidParser_is_signed_integral_typespec(enum MidParser_TypeSpec spec)
{
    return (spec == MIDPARSER_TYPESPEC_CHAR && MidTypes_char_signed) ||
           spec == MIDPARSER_TYPESPEC_SCHAR ||
           (spec == MIDPARSER_TYPESPEC_WCHAR && MidTypes_wchar_signed) ||
           spec == MIDPARSER_TYPESPEC_SHORT || spec == MIDPARSER_TYPESPEC_INT ||
           spec == MIDPARSER_TYPESPEC_LONG ||
           spec == MIDPARSER_TYPESPEC_LONGLONG ||
           spec == MIDPARSER_TYPESPEC_BOOL;
}

bool MidParser_is_unsigned_integral_typespec(enum MidParser_TypeSpec spec)
{
    return MidParser_is_integral_typespec(spec) &&
           !MidParser_is_signed_integral_typespec(spec);
}

bool MidParser_is_floating_typespec(enum MidParser_TypeSpec spec)
{
    return spec == MIDPARSER_TYPESPEC_FLOAT ||
           spec == MIDPARSER_TYPESPEC_DOUBLE ||
           spec == MIDPARSER_TYPESPEC_LONGDOUBLE;
}

void MidParser_Type_deinit(struct MidParser_Type *self)
{
    if (self->spec == MIDPARSER_TYPESPEC_FPTR) {
        MidGen_dyndeinit(&self->fptr->params, MidParser_Type_deinit);
        MidParser_Type_deinit(&self->fptr->ret);
        free(self->fptr);
    } else if (self->spec == MIDPARSER_TYPESPEC_ARRAY) {
        MidParser_Type_deinit(&self->array->elem);
        free(self->array);
    }

    MidGen_dyndeinit(&self->dquals);
}

bool is_ptr_tok(enum MidLexer_TokenType type)
{
    return type == MIDLEXER_TOKENTYPE_MUL;
}

bool is_lv_ref_tok(enum MidLexer_TokenType type)
{
    return type == MIDLEXER_TOKENTYPE_BITWISE_AND;
}

bool is_rv_ref_tok(enum MidLexer_TokenType type)
{
    return type == MIDLEXER_TOKENTYPE_LOGICAL_AND;
}

static struct MidDiag_Diag
unnecessary_qual_warn(const char *qual, const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("unnecessary '%s' qualifier", qual),
        .warn = MIDDIAG_WARN_UNNECESSARY_QUALIFIER,
        .type = MIDDIAG_TYPE_WARNING,
    };
}

static struct MidDiag_Diag ptr_to_ref_err(const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("pointer to a reference is not allowed"),
        .err = MIDDIAG_ERR_PTR_TO_REF,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static struct MidDiag_Diag
missplaced_const_err(const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("missplaced const specifier"),
        .err = MIDDIAG_ERR_MISPLACED_QUALIFIER,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static struct MidDiag_Diag type_alr_const_err(const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("type is already a reference"),
        .err = MIDDIAG_ERR_TYPE_ALREADY_REF,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static struct MidDiag_Diag expected_paren(bool left,
                                          const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("expected '%c'", left ? '(' : ')'),
        .err = MIDDIAG_ERR_MISSING_PAREN,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static struct MidDiag_Diag spec_unsignable_err(const char *type_name,
                                               const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str(
            "type '%s' cannot be made signed or unsigned", type_name),
        .err = MIDDIAG_ERR_TYPE_UNSIGNABLE,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static struct MidDiag_Diag bad_qual_err(const char *type_name,
                                        const struct MidLexer_Token *tok)
{
    return (struct MidDiag_Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = MidPrint_fmt_to_str("bad qualifier '%s'", type_name),
        .err = MIDDIAG_ERR_TYPE_UNSIGNABLE,
        .type = MIDDIAG_TYPE_ERROR,
    };
}

static enum MidParser_TypeSpec
make_spec_signed(enum MidParser_TypeSpec spec, const struct MidLexer_Token *tok,
                 struct MidDiag_DiagVec *diags)
{
    switch (spec) {
    case MIDPARSER_TYPESPEC_CHAR:
    case MIDPARSER_TYPESPEC_SCHAR:
    case MIDPARSER_TYPESPEC_UCHAR:
        return MIDPARSER_TYPESPEC_SCHAR;

    case MIDPARSER_TYPESPEC_SHORT:
    case MIDPARSER_TYPESPEC_USHORT:
        return MIDPARSER_TYPESPEC_SHORT;

    case MIDPARSER_TYPESPEC_INT:
    case MIDPARSER_TYPESPEC_UINT:
        return MIDPARSER_TYPESPEC_INT;

    case MIDPARSER_TYPESPEC_LONG:
    case MIDPARSER_TYPESPEC_ULONG:
        return MIDPARSER_TYPESPEC_LONG;

    case MIDPARSER_TYPESPEC_LONGLONG:
    case MIDPARSER_TYPESPEC_ULONGLONG:
        return MIDPARSER_TYPESPEC_LONGLONG;

    default:
        MidGen_dynpush(diags,
                    spec_unsignable_err(MidParser_typespec_to_str(spec), tok));
        return spec;
    }
}

static enum MidParser_TypeSpec
make_spec_unsigned(enum MidParser_TypeSpec spec,
                   const struct MidLexer_Token *tok,
                   struct MidDiag_DiagVec *diags)
{
    switch (spec) {
    case MIDPARSER_TYPESPEC_CHAR:
    case MIDPARSER_TYPESPEC_SCHAR:
    case MIDPARSER_TYPESPEC_UCHAR:
        return MIDPARSER_TYPESPEC_UCHAR;

    case MIDPARSER_TYPESPEC_SHORT:
    case MIDPARSER_TYPESPEC_USHORT:
        return MIDPARSER_TYPESPEC_USHORT;

    case MIDPARSER_TYPESPEC_INT:
    case MIDPARSER_TYPESPEC_UINT:
        return MIDPARSER_TYPESPEC_UINT;

    case MIDPARSER_TYPESPEC_LONG:
    case MIDPARSER_TYPESPEC_ULONG:
        return MIDPARSER_TYPESPEC_ULONG;

    case MIDPARSER_TYPESPEC_LONGLONG:
    case MIDPARSER_TYPESPEC_ULONGLONG:
        return MIDPARSER_TYPESPEC_ULONGLONG;

    default:
        MidGen_dynpush(diags,
                    spec_unsignable_err(MidParser_typespec_to_str(spec), tok));
        return spec;
    }
}

static enum MidParser_TypeSpec
make_spec_short(enum MidParser_TypeSpec spec, const struct MidLexer_Token *tok,
                struct MidDiag_DiagVec *diags)
{
    switch (spec) {
    case MIDPARSER_TYPESPEC_INT:
        return MIDPARSER_TYPESPEC_SHORT;
    case MIDPARSER_TYPESPEC_UINT:
        return MIDPARSER_TYPESPEC_USHORT;

    default:
        MidGen_dynpush(diags, bad_qual_err("short", tok));
        return spec;
    }
}

static enum MidParser_TypeSpec
make_spec_long(enum MidParser_TypeSpec spec, const struct MidLexer_Token *tok,
               struct MidDiag_DiagVec *diags)
{
    switch (spec) {
    case MIDPARSER_TYPESPEC_INT:
        return MIDPARSER_TYPESPEC_LONG;
    case MIDPARSER_TYPESPEC_UINT:
        return MIDPARSER_TYPESPEC_ULONG;
    case MIDPARSER_TYPESPEC_DOUBLE:
        return MIDPARSER_TYPESPEC_LONGDOUBLE;

    default:
        MidGen_dynpush(diags, bad_qual_err("long", tok));
        return spec;
    }
}

static enum MidParser_TypeSpec
make_spec_longlong(enum MidParser_TypeSpec spec,
                   const struct MidLexer_Token *tok,
                   struct MidDiag_DiagVec *diags)
{
    switch (spec) {
    case MIDPARSER_TYPESPEC_INT:
        return MIDPARSER_TYPESPEC_LONGLONG;
    case MIDPARSER_TYPESPEC_UINT:
        return MIDPARSER_TYPESPEC_ULONGLONG;

    default:
        MidGen_dynpush(diags, bad_qual_err("long long", tok));
        return spec;
    }
}

void MidParser_set_squal_flag(struct MidParser_TypeStorQual *qual,
                              enum MidLexer_TokenType type)
{
    switch (type) {
    case MIDLEXER_TOKENTYPE_STATIC:
        qual->is_static = true;
        break;

    case MIDLEXER_TOKENTYPE_CONSTEXPR:
        qual->is_constexpr = true;
        break;

    case MIDLEXER_TOKENTYPE_TYPEDEF:
        qual->is_typedef = true;
        break;

    default:
        MID_CRASH("token is not a storage qualifier");
    }
}

void MidParser_set_dqual_flag(struct MidParser_TypeDataQual *qual,
                              enum MidLexer_TokenType type)
{
    switch (type) {
    case MIDLEXER_TOKENTYPE_CONST:
        qual->is_const = true;
        break;

    case MIDLEXER_TOKENTYPE_VOLATILE:
        qual->is_volatile = true;
        break;

    default:
        MID_CRASH("token is not a data qualifier");
    }
}

mid_isize MidParser_parse_quals(const struct MidLexer_Token *toks, mid_isize start,
                              struct MidParser_TypeStorQual *squals,
                              struct MidParser_TypeDataQual *dquals)
{
    mid_isize i;
    for (i = start; MidLexer_is_typequal(toks[i].type); ++i) {
        if (MidLexer_is_typestorqual(toks[i].type))
            MidParser_set_squal_flag(squals, toks[i].type);
        else
            MidParser_set_dqual_flag(dquals, toks[i].type);
    }

    return i;
}

static struct MidParser_Type
type_name_type(const struct MidLexer_Token *toks, mid_isize start,
               mid_isize *out_end, struct MidSema_Scope *scope,
               struct MidParser_Allocators *allocs,
               struct MidDiag_DiagVec *diags)
{
    assert(toks[start].type == MIDLEXER_TOKENTYPE_IDENTIFIER);

    auto ident = MidSema_find_ident_const(scope, toks[start].ident);
    if (!MidSema_ident_is_tmplt(ident->type)) {
        if (out_end)
            *out_end = start + 1;
        return MidSema_type_name_type(scope, toks[start].ident);
    }

    // the type is a template and therefore we need to parse the template
    // arguments. example:
    //  Type<...>
    //  ^
    // toks[start]
    mid_isize l_angle = start + 1;
    mid_isize r_angle;
    struct MidParser_TmpltArgVec args = MidParser_parse_tmplt_args(
        toks, l_angle, &r_angle, scope, allocs, diags);
    if (out_end)
        *out_end = r_angle + 1;

    printf("n args = %" PRIisz "\n", args.len);
    auto tmplt = ident->decl->parent;
    struct MidParser_Type ret =
        MidSema_instantiate_class_tmplt(tmplt, &args, allocs);

    MidGen_dyndeinit(&args, MidParser_TmpltArg_deinit);
    return ret;
}

// parses the type specifier and its preceding qualifiers
// static const int *const &x
// ^^^^^^^^^^^^^^^^
struct MidParser_Type
MidParser_parse_base(const struct MidLexer_Token *toks, mid_isize start,
                     mid_isize *out_end, struct MidSema_Scope *scope,
                     struct MidParser_Allocators *allocs,
                     struct MidDiag_DiagVec *diags)
{
    struct MidParser_Type ret = {};

    mid_isize i = start;

    // this could definitely be written way better

    // point to the token holding the modifier
    const struct MidLexer_Token *is_signed = NULL;
    const struct MidLexer_Token *is_unsigned = NULL;
    const struct MidLexer_Token *is_short = NULL;
    const struct MidLexer_Token *is_long = NULL;
    const struct MidLexer_Token *is_longlong = NULL;

    struct MidParser_TypeDataQual dquals = {};
    struct MidParser_TypeStorQual squals = {};

    bool spec_is_typedef = false;
    bool missing_spec = true;

    for (; MidLexer_is_typequal(toks[i].type) ||
           MidLexer_is_typemod(toks[i].type) ||
           tok_is_type_spec(scope, &toks[i]) ||
           tok_is_namespace_name(scope, &toks[i]) ||
           toks[i].type == MIDLEXER_TOKENTYPE_SCOPE_RES;
         ++i) {
        if (MidLexer_is_typedataqual(toks[i].type)) {
            MidParser_set_dqual_flag(&dquals, toks[i].type);
        } else if (toks[i].type == MIDLEXER_TOKENTYPE_SIGNED) {
            if (is_signed)
                MidGen_dynpush(diags, unnecessary_qual_warn("signed", &toks[i]));
            if (is_unsigned)
                MidGen_dynpush(diags, bad_qual_err("signed", &toks[i]));
            is_signed = &toks[i];
        } else if (toks[i].type == MIDLEXER_TOKENTYPE_UNSIGNED) {
            if (is_unsigned)
                MidGen_dynpush(diags, unnecessary_qual_warn("unsigned", &toks[i]));
            if (is_signed)
                MidGen_dynpush(diags, bad_qual_err("unsigned", &toks[i]));
            is_unsigned = &toks[i];
        } else if (toks[i].type == MIDLEXER_TOKENTYPE_SHORT) {
            if (is_short)
                MidGen_dynpush(diags, unnecessary_qual_warn("short", &toks[i]));
            else if (is_long || is_longlong)
                MidGen_dynpush(diags, bad_qual_err("short", &toks[i]));
            is_short = &toks[i];
        } else if (toks[i].type == MIDLEXER_TOKENTYPE_LONG) {
            if (is_longlong || is_short) {
                MidGen_dynpush(diags, bad_qual_err("long", &toks[i]));
            } else if (is_long) {
                is_longlong = &toks[i];
                is_long = NULL;
            } else {
                is_long = &toks[i];
            }
        } else if (MidLexer_is_typequal(toks[i].type)) {
            MidParser_set_squal_flag(&squals, toks[i].type);
        } else if (missing_spec) {
            missing_spec = false;
            auto res = MidParser_parse_scope_res(toks, i, &i, scope, diags);
            if (toks[i].type == MIDLEXER_TOKENTYPE_IDENTIFIER) {
                ret = type_name_type(toks, i, &i, res, allocs, diags);
                --i;
            } else {
                ret = MidParser_toktype_to_type(toks[i].type);
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
        MidGen_dynpush(&ret.dquals, dquals);

    // short, long and long long don't need a type spec
    if ((is_short || is_long || is_longlong) && missing_spec) {
        missing_spec = false;
        ret.spec = MIDPARSER_TYPESPEC_INT;
    } else if (missing_spec) {
        struct MidDiag_Diag err = {.pos = toks[start].pos,
                                   .line = toks[start].line,
                                   .msg = strdup("expected a type specifier"),
                                   .err = MIDDIAG_ERR_MISSING_TYPESPEC,
                                   .type = MIDDIAG_TYPE_ERROR};
        MidGen_dynpush(diags, err);
        ret.spec = MIDPARSER_TYPESPEC_INT; // default to int
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

static struct MidParser_Type
parse_recursive_part(const struct MidLexer_Token *toks, mid_isize start,
                     mid_isize min, mid_isize *out_end, struct MidSema_Scope *scope,
                     const struct MidParser_TypeStorQual *squals,
                     struct MidParser_Allocators *allocs,
                     struct MidDiag_DiagVec *diags);

// returns the end of the function ptr
// void (*func_ptr)(int, float)
//      ^         ^           ^
//    lparen    rparen      return
static mid_isize parse_fptr(struct MidParser_Type *type,
                          const struct MidLexer_Token *toks, mid_isize lparen,
                          mid_isize rparen, mid_isize min,
                          struct MidSema_Scope *scope,
                          struct MidParser_Allocators *allocs,
                          struct MidDiag_DiagVec *diags)
{
    mid_isize p_lparen = rparen + 1;
    mid_isize p_rparen = MidParser_find_twin_paren(toks, p_lparen, MID_ISIZE_MAX);

    type->spec = MIDPARSER_TYPESPEC_FPTR;
    type->fptr = Mid_malloc(sizeof(*type->fptr));
    type->fptr->ret =
        parse_recursive_part(toks, lparen - 1, min, NULL, scope,
                             &(struct MidParser_TypeStorQual){}, allocs, diags);
    type->fptr->params = (struct MidParser_TypeVec){};

    mid_isize i = p_lparen + 1;
    while (i < p_rparen) {
        MidGen_dynpush(&type->fptr->params,
                    MidParser_parse_type(toks, i, &i, scope, NULL, false,
                                         allocs, diags));

        if (toks[i].type != MIDLEXER_TOKENTYPE_COMMA &&
            toks[i].type != MIDLEXER_TOKENTYPE_R_PAREN) {
            MidGen_dynpush(diags, expected_paren(false, toks));
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
static mid_isize parse_array(struct MidParser_Type *type,
                           const struct MidLexer_Token *toks, mid_isize lparen,
                           mid_isize rparen, mid_isize min,
                           struct MidSema_Scope *scope,
                           struct MidParser_Allocators *allocs,
                           struct MidDiag_DiagVec *diags)
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

static struct MidParser_Type
parse_recursive_part(const struct MidLexer_Token *toks, mid_isize start,
                     mid_isize min, mid_isize *out_end, struct MidSema_Scope *scope,
                     const struct MidParser_TypeStorQual *squals,
                     struct MidParser_Allocators *allocs,
                     struct MidDiag_DiagVec *diags)
{
    struct MidParser_Type ret = {.squals = *squals};

    struct MidParser_TypeDataQual dquals = {};

    mid_isize i;
    for (i = start;
         i >= min &&
         (MidLexer_is_typedataqual(toks[i].type) || is_ptr_tok(toks[i].type) ||
          is_lv_ref_tok(toks[i].type) || is_rv_ref_tok(toks[i].type));
         --i) {
        if (MidLexer_is_typedataqual(toks[i].type)) {
            MidParser_set_dqual_flag(&dquals, toks[i].type);
        } else if (is_ptr_tok(toks[i].type)) {
            MidGen_dynpush(&ret.dquals, dquals);
            dquals = (struct MidParser_TypeDataQual){};
        } else if (is_lv_ref_tok(toks[i].type)) {
            if (ret.lv_ref || ret.rv_ref)
                MidGen_dynpush(diags, type_alr_const_err(&toks[i]));
            else if (ret.dquals.len > 0)
                MidGen_dynpush(diags, ptr_to_ref_err(&toks[i]));
            else if (dquals.is_const)
                MidGen_dynpush(diags, missplaced_const_err(&toks[i]));
            else
                ret.lv_ref = true;
        } else {
            if (ret.lv_ref || ret.rv_ref)
                MidGen_dynpush(diags, type_alr_const_err(&toks[i]));
            else if (ret.dquals.len > 0)
                MidGen_dynpush(diags, ptr_to_ref_err(&toks[i]));
            else if (dquals.is_const)
                MidGen_dynpush(diags, missplaced_const_err(&toks[i]));
            else
                ret.rv_ref = true;
        }
    }

    // end is non inclusive
    mid_isize end = start + 1;

    if (toks[i].type == MIDLEXER_TOKENTYPE_L_PAREN) {
        mid_isize rparen = MidParser_find_twin_paren(toks, i, MID_ISIZE_MAX);
        if (rparen == -1) {
            MidGen_dynpush(diags, expected_paren(false, &toks[i]));
        } else {
            if (toks[rparen + 1].type == MIDLEXER_TOKENTYPE_L_PAREN)
                end = parse_fptr(&ret, toks, i, rparen, min, scope, allocs,
                                 diags);
            else if (toks[rparen + 1].type == MIDLEXER_TOKENTYPE_L_SQBRACKET)
                end = parse_array(&ret, toks, i, rparen, min, scope, allocs,
                                  diags);
        }
    } else if (toks[end].type == MIDLEXER_TOKENTYPE_IDENTIFIER) {
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
mid_isize find_type_center(const struct MidLexer_Token *toks, mid_isize start)
{
    mid_isize i = start;
    while (toks[i].type == MIDLEXER_TOKENTYPE_L_PAREN ||
           MidLexer_is_typedataqual(toks[i].type) || is_ptr_tok(toks[i].type) ||
           is_lv_ref_tok(toks[i].type) || is_rv_ref_tok(toks[i].type))
        ++i;

    if (toks[i].type == MIDLEXER_TOKENTYPE_IDENTIFIER)
        ++i;

    return i - 1;
}

struct MidParser_TypeFPtr
MidParser_copy_fptr_type(const struct MidParser_TypeFPtr *fptr)
{
    struct MidParser_TypeFPtr ret = {.has_ellipsis = fptr->has_ellipsis};
    ret.ret = MidParser_copy_type(&fptr->ret);

    for (mid_isize i = 0; i < fptr->params.len; ++i)
        MidGen_dynpush(&ret.params, MidParser_copy_type(&fptr->params.arr[i]));

    return ret;
}

struct MidParser_TypeArray
MidParser_copy_array_type(const struct MidParser_TypeArray *arr)
{
    struct MidParser_TypeArray ret = {};
    ret.elem = MidParser_copy_type(&arr->elem);
    ret.len = arr->len;
    return ret;
}

static void add_base(struct MidParser_Type *type,
                     const struct MidParser_Type *base,
                     const struct MidLexer_Token *type_start,
                     struct MidDiag_DiagVec *diags)
{
    if (type->spec == MIDPARSER_TYPESPEC_FPTR) {
        add_base(&type->fptr->ret, base, type_start, diags);
    } else if (type->spec == MIDPARSER_TYPESPEC_ARRAY) {
        add_base(&type->array->elem, base, type_start, diags);
    } else {
        if (base->spec == MIDPARSER_TYPESPEC_FPTR) {
            type->fptr = Mid_malloc(sizeof(*type->fptr));
            *type->fptr = MidParser_copy_fptr_type(base->fptr);
        } else if (base->spec == MIDPARSER_TYPESPEC_ARRAY) {
            type->array = Mid_malloc(sizeof(*type->array));
            *type->array = MidParser_copy_array_type(base->array);
        } else if (MidParser_is_typespec_named(base->spec)) {
            type->named = base->named;
        } else if (base->spec == MIDPARSER_TYPESPEC_FUNC) {
            type->func = base->func;
        }

        if (type->dquals.len > 0 && (base->lv_ref || base->rv_ref))
            MidGen_dynpush(diags, ptr_to_ref_err(type_start));

        for (mid_isize i = 0; i < base->dquals.len; ++i)
            MidGen_dynpush(&type->dquals, base->dquals.arr[i]);
        type->spec = base->spec;
        type->squals = base->squals;
        type->lv_ref |= base->lv_ref;
        type->rv_ref |= base->rv_ref;
    }
}

struct MidParser_Type MidParser_parse_type(
    const struct MidLexer_Token *toks, mid_isize start, mid_isize *out_end,
    struct MidSema_Scope *scope, mid_isize *out_declname, bool is_type_id,
    struct MidParser_Allocators *allocs, struct MidDiag_DiagVec *diags)
{
    mid_isize i;
    auto base = MidParser_parse_base(toks, start, &i, scope, allocs, diags);

    auto ret =
        MidParser_parse_type_no_base(toks, i, out_end, &base, scope,
                                     out_declname, is_type_id, allocs, diags);

    MidParser_Type_deinit(&base);
    return ret;
}

struct MidParser_Type MidParser_parse_type_no_base(
    const struct MidLexer_Token *toks, mid_isize start, mid_isize *out_end,
    const struct MidParser_Type *base, struct MidSema_Scope *scope,
    mid_isize *out_declname, bool is_type_id, struct MidParser_Allocators *allocs,
    struct MidDiag_DiagVec *diags)
{
    mid_isize c = find_type_center(toks, start);

    bool has_declname = toks[c].type == MIDLEXER_TOKENTYPE_IDENTIFIER &&
                        !MidSema_is_type_name(scope, toks[c].ident);
    if (has_declname && is_type_id)
        MidGen_dynpush(diags,
                    MidDiag_type_id_w_name_err(&toks[c], MIDDIAG_ERR_BAD_TYPE));

    auto ret = parse_recursive_part(toks, c - has_declname, start, out_end,
                                    scope, &base->squals, allocs, diags);
    add_base(&ret, base, &toks[start], diags);

    if (out_declname)
        *out_declname = has_declname ? c : -1;
    return ret;
}

mid_isize MidParser_n_indir(const struct MidParser_Type *type)
{
    return type->dquals.len - 1;
}

struct MidParser_Type MidParser_copy_type(const struct MidParser_Type *type)
{
    struct MidParser_Type ret = {
        .spec = type->spec,
        .squals = type->squals,
        .lv_ref = type->lv_ref,
        .rv_ref = type->rv_ref,
    };

    for (mid_isize i = 0; i < type->dquals.len; ++i)
        MidGen_dynpush(&ret.dquals, type->dquals.arr[i]);

    if (type->spec == MIDPARSER_TYPESPEC_FPTR) {
        ret.fptr = Mid_malloc(sizeof(*ret.fptr));
        *ret.fptr = MidParser_copy_fptr_type(type->fptr);
    } else if (type->spec == MIDPARSER_TYPESPEC_ARRAY) {
        ret.array = Mid_malloc(sizeof(*ret.array));
        *ret.array = MidParser_copy_array_type(type->array);
    } else if (MidParser_is_typespec_named(type->spec)) {
        ret.named = type->named;
    } else if (type->spec == MIDPARSER_TYPESPEC_FUNC) {
        ret.func = type->func;
    }

    return ret;
}

struct MidParser_Type MidParser_ref_type(const struct MidParser_Type *type,
                                         bool *out_failed)
{
    auto ret = MidParser_copy_type(type);

    if (!ret.lv_ref && !ret.rv_ref) {
        MidGen_dynpush(&ret.dquals, (struct MidParser_TypeDataQual){});
        if (out_failed)
            *out_failed = false;
    } else if (out_failed) {
        *out_failed = true;
    }

    return ret;
}

struct MidParser_Type MidParser_deref_type(const struct MidParser_Type *type,
                                           bool *out_failed)
{
    auto ret = MidParser_copy_type(type);

    if (ret.dquals.len > 1) {
        // the first element holds the top most ptr
        MidGen_dynremove(&ret.dquals, 0);
        if (out_failed)
            *out_failed = false;
    } else if (out_failed) {
        *out_failed = true;
    }

    return ret;
}

struct MidParser_Type MidParser_toktype_to_type(enum MidLexer_TokenType type)
{
    struct MidParser_Type ret = {};
    MidGen_dynpush(&ret.dquals, (struct MidParser_TypeDataQual){});

    switch (type) {
    case MIDLEXER_TOKENTYPE_VOID:
        ret.spec = MIDPARSER_TYPESPEC_VOID;
        break;

    case MIDLEXER_TOKENTYPE_CHAR:
        ret.spec = MIDPARSER_TYPESPEC_CHAR;
        break;

    case MIDLEXER_TOKENTYPE_WCHAR:
        ret.spec = MIDPARSER_TYPESPEC_WCHAR;
        break;

    case MIDLEXER_TOKENTYPE_CHAR16:
        ret.spec = MIDPARSER_TYPESPEC_CHAR16;
        break;

    case MIDLEXER_TOKENTYPE_CHAR32:
        ret.spec = MIDPARSER_TYPESPEC_CHAR32;
        break;

    case MIDLEXER_TOKENTYPE_INT:
        ret.spec = MIDPARSER_TYPESPEC_INT;
        break;

    case MIDLEXER_TOKENTYPE_FLOAT:
        ret.spec = MIDPARSER_TYPESPEC_FLOAT;
        break;

    case MIDLEXER_TOKENTYPE_DOUBLE:
        ret.spec = MIDPARSER_TYPESPEC_DOUBLE;
        break;

    case MIDLEXER_TOKENTYPE_BOOL:
        ret.spec = MIDPARSER_TYPESPEC_BOOL;
        break;

    case MIDLEXER_TOKENTYPE_STRUCT:
    case MIDLEXER_TOKENTYPE_CLASS:
        ret.spec = MIDPARSER_TYPESPEC_CLASS;
        break;

    case MIDLEXER_TOKENTYPE_UNION:
        ret.spec = MIDPARSER_TYPESPEC_UNION;
        break;

    case MIDLEXER_TOKENTYPE_ENUM:
        ret.spec = MIDPARSER_TYPESPEC_ENUM;
        break;

    case MIDLEXER_TOKENTYPE_AUTO:
        ret.spec = MIDPARSER_TYPESPEC_AUTO;
        break;

    default:
        MID_CRASH("can only convert POD type spec tokens to MidParser_Type");
    }

    return ret;
}

static void type_to_str_impl(const struct MidParser_Type *type,
                             struct Mid_Dynstr *str);

static void fptr_to_str(const struct MidParser_Type *type, struct Mid_Dynstr *str)
{
    type_to_str_impl(&type->fptr->ret, str);
    MidDynstr_append_char(str, ' ');

    MidDynstr_append_char(str, '(');
    for (mid_isize i = 0; i < MidParser_n_indir(type) + 1; ++i)
        MidDynstr_append_char(str, '*');
    if (type->lv_ref)
        MidDynstr_append_char(str, '&');
    else if (type->rv_ref)
        MidDynstr_append(str, "&&");
    MidDynstr_append_char(str, ')');

    MidDynstr_append_char(str, '(');
    for (mid_isize i = 0; i < type->fptr->params.len; ++i) {
        if (i > 0)
            MidDynstr_append(str, ", ");
        type_to_str_impl(&type->fptr->params.arr[i], str);
    }
    MidDynstr_append_char(str, ')');
}

static void array_to_str(const struct MidParser_Type *type, struct Mid_Dynstr *str)
{
    type_to_str_impl(&type->array->elem, str);
    MidDynstr_append_printf(str, "[%" PRIu64 "]", type->array->len);
}

static void dquals_to_str(const struct MidParser_TypeDataQual *dquals,
                          struct Mid_Dynstr *str, bool leading_space,
                          bool trailing_space)
{
    if (dquals->is_const) {
        if (leading_space)
            MidDynstr_append_char(str, ' ');
        MidDynstr_append(str, "const");
        if (trailing_space)
            MidDynstr_append_char(str, ' ');
    }
}

static void regular_type_to_str(const struct MidParser_Type *type,
                                struct Mid_Dynstr *str)
{
    dquals_to_str(&type->dquals.arr[type->dquals.len - 1], str, false, true);
    MidDynstr_append(str, MidParser_typespec_to_str(type->spec));
    if (MidParser_is_typespec_named(type->spec))
        MidDynstr_append_printf(
            str, " %s", type->named.parent->idents.arr[type->named.idx].name);

    for (mid_isize i = MidParser_n_indir(type); i > 0; --i) {
        MidDynstr_append_char(str, '*');
        dquals_to_str(&type->dquals.arr[i - 1], str, true, false);
    }

    if (type->lv_ref)
        MidDynstr_append_char(str, '&');
    else if (type->rv_ref)
        MidDynstr_append(str, "&&");
}

static void type_to_str_impl(const struct MidParser_Type *type,
                             struct Mid_Dynstr *str)
{
    if (type->spec == MIDPARSER_TYPESPEC_FPTR)
        fptr_to_str(type, str);
    else if (type->spec == MIDPARSER_TYPESPEC_ARRAY)
        array_to_str(type, str);
    else if (type->spec == MIDPARSER_TYPESPEC_INVALID)
        MidDynstr_append(str, "INVALID-TYPE");
    else
        regular_type_to_str(type, str);
}

char *MidParser_type_to_str(const struct MidParser_Type *type)
{
    struct Mid_Dynstr str = MidDynstr_init();
    type_to_str_impl(type, &str);
    return str.str;
}

bool MidParser_valid_type_start(const struct MidLexer_Token *toks, mid_isize idx,
                                const struct MidSema_Scope *scope)
{
    if (MidLexer_is_typemod(toks[idx].type) ||
        MidLexer_is_typequal(toks[idx].type))
        return true;

    struct MidDiag_DiagVec tmp = {};
    mid_isize res_end;
    auto res =
        MidParser_parse_scope_res_const(toks, idx, &res_end, scope, &tmp);
    MidGen_dyndeinit(&tmp);

    return tok_is_type_spec(res, &toks[res_end]);
}

enum MidParser_TypeSpec MidParser_uint_type_of_width(i32 bytes)
{
    if (MidTypes_char_size == bytes)
        return MIDPARSER_TYPESPEC_UCHAR;
    else if (MidTypes_short_size == bytes)
        return MIDPARSER_TYPESPEC_USHORT;
    else if (MidTypes_int_size == bytes)
        return MIDPARSER_TYPESPEC_UINT;
    else if (MidTypes_long_size == bytes)
        return MIDPARSER_TYPESPEC_ULONG;
    else if (MidTypes_longlong_size == bytes)
        return MIDPARSER_TYPESPEC_ULONGLONG;
    else
        return MIDPARSER_TYPESPEC_INVALID;
}

enum MidParser_TypeSpec MidParser_sint_type_of_width(i32 bytes)
{
    if (MidTypes_char_size == bytes)
        return MIDPARSER_TYPESPEC_SCHAR;
    else if (MidTypes_short_size == bytes)
        return MIDPARSER_TYPESPEC_SHORT;
    else if (MidTypes_int_size == bytes)
        return MIDPARSER_TYPESPEC_INT;
    else if (MidTypes_long_size == bytes)
        return MIDPARSER_TYPESPEC_LONG;
    else if (MidTypes_longlong_size == bytes)
        return MIDPARSER_TYPESPEC_LONGLONG;
    else
        return MIDPARSER_TYPESPEC_INVALID;
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

i32 MidParser_typespec_conv_rank(enum MidParser_TypeSpec spec)
{
    switch (spec) {
    case MIDPARSER_TYPESPEC_BOOL:
        return 10;

    case MIDPARSER_TYPESPEC_CHAR:
    case MIDPARSER_TYPESPEC_SCHAR:
    case MIDPARSER_TYPESPEC_UCHAR:
        return 20;

    case MIDPARSER_TYPESPEC_SHORT:
    case MIDPARSER_TYPESPEC_USHORT:
        return 30;

    case MIDPARSER_TYPESPEC_INT:
    case MIDPARSER_TYPESPEC_UINT:
        return 40;

    case MIDPARSER_TYPESPEC_LONG:
    case MIDPARSER_TYPESPEC_ULONG:
        return 50;

    case MIDPARSER_TYPESPEC_LONGLONG:
    case MIDPARSER_TYPESPEC_ULONGLONG:
        return 60;

    case MIDPARSER_TYPESPEC_FLOAT:
        return 70;

    case MIDPARSER_TYPESPEC_DOUBLE:
        return 80;

    case MIDPARSER_TYPESPEC_LONGDOUBLE:
        return 90;

    case MIDPARSER_TYPESPEC_WCHAR:
        if (MidTypes_wchar_signed)
            return MidParser_typespec_conv_rank(
                MidParser_sint_type_of_width(MidTypes_wchar_size));
        else
            return MidParser_typespec_conv_rank(
                MidParser_uint_type_of_width(MidTypes_wchar_size));
    case MIDPARSER_TYPESPEC_CHAR16:
        return MidParser_typespec_conv_rank(
            MidParser_uint_type_of_width(16 / 8));
    case MIDPARSER_TYPESPEC_CHAR32:
        return MidParser_typespec_conv_rank(
            MidParser_uint_type_of_width(32 / 8));

    default:
        printf("type = %d\n", spec);
        MID_CRASH("type doesn't have a rank");
    }
}

u64 MidParser_integral_max(enum MidParser_TypeSpec spec)
{
    switch (spec) {
    case MIDPARSER_TYPESPEC_CHAR:
        return MidTypes_char_signed ? MidTypes_char_smax : MidTypes_char_umax;
    case MIDPARSER_TYPESPEC_SCHAR:
        return MidTypes_char_smax;
    case MIDPARSER_TYPESPEC_UCHAR:
        return MidTypes_char_umax;
    case MIDPARSER_TYPESPEC_WCHAR:
        if (MidTypes_wchar_signed)
            return MidParser_integral_max(
                MidParser_sint_type_of_width(MidTypes_wchar_size));
        else
            return MidParser_integral_max(
                MidParser_uint_type_of_width(MidTypes_wchar_size));
    case MIDPARSER_TYPESPEC_CHAR16:
        return MidParser_integral_max(MidParser_uint_type_of_width(16 / 8));
    case MIDPARSER_TYPESPEC_CHAR32:
        return MidParser_integral_max(MidParser_uint_type_of_width(32 / 8));

    case MIDPARSER_TYPESPEC_SHORT:
        return MidTypes_short_smax;
    case MIDPARSER_TYPESPEC_USHORT:
        return MidTypes_short_umax;

    case MIDPARSER_TYPESPEC_INT:
        return MidTypes_int_smax;
    case MIDPARSER_TYPESPEC_UINT:
        return MidTypes_int_umax;

    case MIDPARSER_TYPESPEC_LONG:
        return MidTypes_long_smax;
    case MIDPARSER_TYPESPEC_ULONG:
        return MidTypes_long_umax;

    case MIDPARSER_TYPESPEC_LONGLONG:
        return MidTypes_longlong_smax;
    case MIDPARSER_TYPESPEC_ULONGLONG:
        return MidTypes_longlong_umax;

    case MIDPARSER_TYPESPEC_BOOL:
        return 1;

    default:
        assert(!MidParser_is_integral_typespec(spec));
        MID_CRASH("spec isn't integral");
    }
}

i64 MidParser_integral_min(enum MidParser_TypeSpec spec)
{
    switch (spec) {
    case MIDPARSER_TYPESPEC_CHAR:
        return MidTypes_char_signed ? MidTypes_char_smin : 0;
    case MIDPARSER_TYPESPEC_SCHAR:
        return MidTypes_char_smin;
    case MIDPARSER_TYPESPEC_UCHAR:
        return 0;
    case MIDPARSER_TYPESPEC_WCHAR:
        if (MidTypes_wchar_signed)
            return MidParser_integral_min(
                MidParser_sint_type_of_width(MidTypes_wchar_size));
        else
            return 0;
    case MIDPARSER_TYPESPEC_CHAR16:
        return 0;
    case MIDPARSER_TYPESPEC_CHAR32:
        return 0;

    case MIDPARSER_TYPESPEC_SHORT:
        return MidTypes_short_smin;
    case MIDPARSER_TYPESPEC_USHORT:
        return 0;

    case MIDPARSER_TYPESPEC_INT:
        return MidTypes_int_smin;
    case MIDPARSER_TYPESPEC_UINT:
        return 0;

    case MIDPARSER_TYPESPEC_LONG:
        return MidTypes_long_smin;
    case MIDPARSER_TYPESPEC_ULONG:
        return 0;

    case MIDPARSER_TYPESPEC_LONGLONG:
        return MidTypes_longlong_smin;
    case MIDPARSER_TYPESPEC_ULONGLONG:
        return 0;

    case MIDPARSER_TYPESPEC_BOOL:
        return 0;

    default:
        assert(!MidParser_is_integral_typespec(spec));
        MID_CRASH("spec isn't integral");
    }
}

enum MidParser_TypeSpec MidParser_integral_prom(enum MidParser_TypeSpec spec)
{
    assert(MidParser_is_integral_typespec(spec));

    if (spec == MIDPARSER_TYPESPEC_BOOL)
        return MIDPARSER_TYPESPEC_INT;

    i32 spec_rank = MidParser_typespec_conv_rank(spec);
    i32 int_rank = MidParser_typespec_conv_rank(MIDPARSER_TYPESPEC_INT);

    if (spec_rank < int_rank) {
        if (MidParser_integral_max(MIDPARSER_TYPESPEC_INT) >=
                MidParser_integral_max(spec) &&
            MidParser_integral_min(MIDPARSER_TYPESPEC_INT) <=
                MidParser_integral_min(spec))
            return MIDPARSER_TYPESPEC_INT;
        else
            return MIDPARSER_TYPESPEC_UINT;
    } else {
        return spec;
    }
}

bool MidParser_is_fundamental_type(const struct MidParser_Type *type)
{
    return MidParser_n_indir(type) == 0 &&
           (MidParser_is_integral_typespec(type->spec) ||
            MidParser_is_floating_typespec(type->spec));
}

static bool are_fptrs_same(const struct MidParser_TypeFPtr *a,
                           const struct MidParser_TypeFPtr *b)
{
    if (a->params.len != b->params.len)
        return false;
    else if (a->has_ellipsis != b->has_ellipsis)
        return false;
    else if (!MidParser_are_types_same(&a->ret, &b->ret))
        return false;

    for (mid_isize i = 0; i < a->params.len; ++i) {
        if (!MidParser_are_types_same(&a->params.arr[i], &b->params.arr[i]))
            return false;
    }

    return true;
}

static bool are_arrays_same(const struct MidParser_TypeArray *a,
                            const struct MidParser_TypeArray *b)
{
    if (a->len != b->len)
        return false;

    return MidParser_are_types_same(&a->elem, &b->elem);
}

bool MidParser_dquals_same(const struct MidParser_TypeDataQual *a, mid_isize n_a,
                           const struct MidParser_TypeDataQual *b, mid_isize n_b)
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

bool MidParser_squals_same(const struct MidParser_TypeStorQual *a,
                           const struct MidParser_TypeStorQual *b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

bool MidParser_are_types_same(const struct MidParser_Type *a,
                              const struct MidParser_Type *b)
{
    if (a->spec != b->spec)
        return false;
    else if (a->lv_ref != b->lv_ref || a->rv_ref != b->rv_ref)
        return false;
    else if (!MidParser_squals_same(&a->squals, &b->squals))
        return false;
    else if (!MidParser_dquals_same(a->dquals.arr, a->dquals.len, b->dquals.arr,
                                    b->dquals.len))
        return false;
    else if (a->spec == MIDPARSER_TYPESPEC_FPTR)
        return are_fptrs_same(a->fptr, b->fptr);
    else if (a->spec == MIDPARSER_TYPESPEC_ARRAY)
        return are_arrays_same(a->array, b->array);
    else if (MidParser_is_typespec_named(a->spec))
        return a->named.parent == b->named.parent &&
               a->named.idx == b->named.idx;
    else
        return true;
}

struct MidParser_Type MidParser_create_func_type(struct MidSema_Scope *scope,
                                                 const char *name)
{
    struct MidParser_Type ret = {};
    ret.spec = MIDPARSER_TYPESPEC_FUNC;
    ret.func.scope = scope;
    ret.func.name = name;

    MidGen_dynpush(&ret.dquals, ((struct MidParser_TypeDataQual){}));

    return ret;
}

struct MidParser_Type MidParser_create_named_type(struct MidSema_IdentPtr ident,
                                                  enum MidParser_TypeSpec spec)
{
    struct MidParser_Type ret = {.spec = spec};
    ret.named = ident;

    MidGen_dynpush(&ret.dquals, ((struct MidParser_TypeDataQual){}));

    return ret;
}

struct MidParser_Type
MidParser_create_templated_type(struct MidSema_IdentPtr ident)
{
    return MidParser_create_named_type(ident, MIDPARSER_TYPESPEC_TEMPLATED);
}

struct MidParser_Type MidParser_create_unknown_type()
{
    struct MidParser_Type ret = {.spec = MIDPARSER_TYPESPEC_UNKNOWN};

    MidGen_dynpush(&ret.dquals, ((struct MidParser_TypeDataQual){}));

    return ret;
}

bool MidParser_type_is_void(const struct MidParser_Type *type)
{
    return MidParser_n_indir(type) == 0 &&
           type->spec == MIDPARSER_TYPESPEC_VOID;
}

bool MidParser_type_is_void_ptr(const struct MidParser_Type *type)
{
    return MidParser_n_indir(type) == 1 &&
           type->spec == MIDPARSER_TYPESPEC_VOID;
}

bool MidParser_type_is_nullptr_t(const struct MidParser_Type *type)
{
    return MidParser_n_indir(type) == 0 &&
           type->spec == MIDPARSER_TYPESPEC_NULLPTR;
}

bool MidParser_type_is_ref(const struct MidParser_Type *type)
{
    return type->lv_ref || type->rv_ref;
}

bool MidParser_type_is_typecheckable(const struct MidParser_Type *type)
{
    return MidParser_is_typespec_typecheckable(type->spec);
}
