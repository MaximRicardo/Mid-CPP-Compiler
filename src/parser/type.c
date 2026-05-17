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
#include "print.h"
#include "scope.h"
#include "sema/scope.h"
#include "types.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool tok_is_type_spec(const struct Sema_Scope *scope,
                             const struct Lexer_Token *tok)
{
    return Lexer_is_typespec(tok->type) ||
           (tok->type == LEXER_TOKENTYPE_IDENTIFIER &&
            Sema_is_type_name(scope, tok->ident));
}

static bool tok_is_namespace_name(const struct Sema_Scope *scope,
                                  const struct Lexer_Token *tok)
{
    return tok->type == LEXER_TOKENTYPE_IDENTIFIER &&
           Sema_is_namespace_name(scope, tok->ident);
}

bool Parser_is_typespec_named(enum Parser_TypeSpec spec)
{
    return spec == PARSER_TYPESPEC_CLASS || spec == PARSER_TYPESPEC_ENUM ||
           spec == PARSER_TYPESPEC_UNION;
}

enum Parser_TypeSpec Parser_toktype_to_typespec(enum Lexer_TokenType type)
{
    switch (type) {
    case LEXER_TOKENTYPE_VOID:
        return PARSER_TYPESPEC_VOID;

    case LEXER_TOKENTYPE_CHAR:
        return PARSER_TYPESPEC_CHAR;

    case LEXER_TOKENTYPE_WCHAR:
        return PARSER_TYPESPEC_WCHAR;

    case LEXER_TOKENTYPE_CHAR16:
        return PARSER_TYPESPEC_CHAR16;

    case LEXER_TOKENTYPE_CHAR32:
        return PARSER_TYPESPEC_CHAR32;

    case LEXER_TOKENTYPE_INT:
        return PARSER_TYPESPEC_INT;

    case LEXER_TOKENTYPE_FLOAT:
        return PARSER_TYPESPEC_FLOAT;

    case LEXER_TOKENTYPE_DOUBLE:
        return PARSER_TYPESPEC_DOUBLE;

    case LEXER_TOKENTYPE_BOOL:
        return PARSER_TYPESPEC_BOOL;

    default:
        CRASH("token is not a type spec");
    }
}

const char *Parser_typespec_to_str(enum Parser_TypeSpec spec)
{
    switch (spec) {
    case PARSER_TYPESPEC_VOID:
        return "void";

    case PARSER_TYPESPEC_CHAR:
        return "char";
    case PARSER_TYPESPEC_SCHAR:
        return "signed char";
    case PARSER_TYPESPEC_UCHAR:
        return "unsigned char";

    case PARSER_TYPESPEC_SHORT:
        return "short";
    case PARSER_TYPESPEC_USHORT:
        return "unsigned short";

    case PARSER_TYPESPEC_INT:
        return "int";
    case PARSER_TYPESPEC_UINT:
        return "unsigned int";

    case PARSER_TYPESPEC_LONG:
        return "long";
    case PARSER_TYPESPEC_ULONG:
        return "unsigned long";

    case PARSER_TYPESPEC_LONGLONG:
        return "long long";
    case PARSER_TYPESPEC_ULONGLONG:
        return "unsigned long long";

    case PARSER_TYPESPEC_FLOAT:
        return "float";
    case PARSER_TYPESPEC_DOUBLE:
        return "double";
    case PARSER_TYPESPEC_LONGDOUBLE:
        return "long double";

    case PARSER_TYPESPEC_BOOL:
        return "bool";
    case PARSER_TYPESPEC_WCHAR:
        return "wchar_t";
    case PARSER_TYPESPEC_CHAR16:
        return "char16_t";
    case PARSER_TYPESPEC_CHAR32:
        return "char32_t";

    case PARSER_TYPESPEC_AUTO:
        return "auto";

    case PARSER_TYPESPEC_CLASS:
        return "class";
    case PARSER_TYPESPEC_UNION:
        return "union";
    case PARSER_TYPESPEC_ENUM:
        return "enum";

    case PARSER_TYPESPEC_INVALID:
    case PARSER_TYPESPEC_FUNC:
    case PARSER_TYPESPEC_FPTR:
    case PARSER_TYPESPEC_ARRAY:
        printf("spec = %d\n", spec);
        CRASH("can't convert type spec to str");
        return "INVALID-TYPE";
    }
}

bool Parser_is_integral_typespec(enum Parser_TypeSpec spec)
{
    return spec == PARSER_TYPESPEC_CHAR || spec == PARSER_TYPESPEC_SCHAR ||
           spec == PARSER_TYPESPEC_UCHAR || spec == PARSER_TYPESPEC_WCHAR ||
           spec == PARSER_TYPESPEC_CHAR16 || spec == PARSER_TYPESPEC_CHAR32 ||
           spec == PARSER_TYPESPEC_SHORT || spec == PARSER_TYPESPEC_USHORT ||
           spec == PARSER_TYPESPEC_INT || spec == PARSER_TYPESPEC_UINT ||
           spec == PARSER_TYPESPEC_LONG || spec == PARSER_TYPESPEC_ULONG ||
           spec == PARSER_TYPESPEC_LONGLONG ||
           spec == PARSER_TYPESPEC_ULONGLONG || spec == PARSER_TYPESPEC_BOOL;
}

bool Parser_is_floating_typespec(enum Parser_TypeSpec spec)
{
    return spec == PARSER_TYPESPEC_FLOAT || spec == PARSER_TYPESPEC_DOUBLE ||
           spec == PARSER_TYPESPEC_LONGDOUBLE;
}

void Parser_Type_deinit(struct Parser_Type *self)
{
    if (self->spec == PARSER_TYPESPEC_FPTR) {
        gen_dyndeinit(&self->fptr->params, Parser_Type_deinit);
        Parser_Type_deinit(&self->fptr->ret);
        free(self->fptr);
    } else if (self->spec == PARSER_TYPESPEC_ARRAY) {
        Parser_Type_deinit(&self->array->elem);
        free(self->array);
    }

    gen_dyndeinit(&self->dquals);
}

bool is_ptr_tok(enum Lexer_TokenType type)
{
    return type == LEXER_TOKENTYPE_MUL;
}

bool is_lv_ref_tok(enum Lexer_TokenType type)
{
    return type == LEXER_TOKENTYPE_BITWISE_AND;
}

bool is_rv_ref_tok(enum Lexer_TokenType type)
{
    return type == LEXER_TOKENTYPE_LOGICAL_AND;
}

static struct Diag unnecessary_qual_warn(const char *qual,
                                         const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("unnecessary '%s' qualifier", qual),
        .warn = WARNTYPE_UNNECESSARY_QUALIFIER,
        .is_err = false,
    };
}

static struct Diag ptr_to_ref_err(const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("pointer to a reference is not allowed"),
        .err = ERRORTYPE_PTR_TO_REF,
        .is_err = false,
    };
}

static struct Diag missplaced_const_err(const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("missplaced const specifier"),
        .err = ERRORTYPE_MISPLACED_QUALIFIER,
        .is_err = true,
    };
}

static struct Diag type_alr_const_err(const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("type is already a reference"),
        .err = ERRORTYPE_TYPE_ALREADY_REF,
        .is_err = true,
    };
}

static struct Diag expected_paren(bool left, const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("expected '%c'", left ? '(' : ')'),
        .err = ERRORTYPE_MISSING_PAREN,
        .is_err = 1,
    };
}

static struct Diag spec_unsignable_err(const char *type_name,
                                       const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("type '%s' cannot be made signed or unsigned",
                                type_name),
        .err = ERRORTYPE_TYPE_UNSIGNABLE,
        .is_err = true,
    };
}

static struct Diag bad_qual_err(const char *type_name,
                                const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("bad qualifier '%s'", type_name),
        .err = ERRORTYPE_TYPE_UNSIGNABLE,
        .is_err = true,
    };
}

static enum Parser_TypeSpec make_spec_signed(enum Parser_TypeSpec spec,
                                             const struct Lexer_Token *tok,
                                             struct DiagVec *diags)
{
    switch (spec) {
    case PARSER_TYPESPEC_CHAR:
    case PARSER_TYPESPEC_SCHAR:
    case PARSER_TYPESPEC_UCHAR:
        return PARSER_TYPESPEC_SCHAR;

    case PARSER_TYPESPEC_SHORT:
    case PARSER_TYPESPEC_USHORT:
        return PARSER_TYPESPEC_SHORT;

    case PARSER_TYPESPEC_INT:
    case PARSER_TYPESPEC_UINT:
        return PARSER_TYPESPEC_INT;

    case PARSER_TYPESPEC_LONG:
    case PARSER_TYPESPEC_ULONG:
        return PARSER_TYPESPEC_LONG;

    case PARSER_TYPESPEC_LONGLONG:
    case PARSER_TYPESPEC_ULONGLONG:
        return PARSER_TYPESPEC_LONGLONG;

    default:
        gen_dynpush(diags,
                    spec_unsignable_err(Parser_typespec_to_str(spec), tok));
        return spec;
    }
}

static enum Parser_TypeSpec make_spec_unsigned(enum Parser_TypeSpec spec,
                                               const struct Lexer_Token *tok,
                                               struct DiagVec *diags)
{
    switch (spec) {
    case PARSER_TYPESPEC_CHAR:
    case PARSER_TYPESPEC_SCHAR:
    case PARSER_TYPESPEC_UCHAR:
        return PARSER_TYPESPEC_UCHAR;

    case PARSER_TYPESPEC_SHORT:
    case PARSER_TYPESPEC_USHORT:
        return PARSER_TYPESPEC_USHORT;

    case PARSER_TYPESPEC_INT:
    case PARSER_TYPESPEC_UINT:
        return PARSER_TYPESPEC_UINT;

    case PARSER_TYPESPEC_LONG:
    case PARSER_TYPESPEC_ULONG:
        return PARSER_TYPESPEC_ULONG;

    case PARSER_TYPESPEC_LONGLONG:
    case PARSER_TYPESPEC_ULONGLONG:
        return PARSER_TYPESPEC_ULONGLONG;

    default:
        gen_dynpush(diags,
                    spec_unsignable_err(Parser_typespec_to_str(spec), tok));
        return spec;
    }
}

static enum Parser_TypeSpec make_spec_short(enum Parser_TypeSpec spec,
                                            const struct Lexer_Token *tok,
                                            struct DiagVec *diags)
{
    switch (spec) {
    case PARSER_TYPESPEC_INT:
        return PARSER_TYPESPEC_SHORT;
    case PARSER_TYPESPEC_UINT:
        return PARSER_TYPESPEC_USHORT;

    default:
        gen_dynpush(diags, bad_qual_err("short", tok));
        return spec;
    }
}

static enum Parser_TypeSpec make_spec_long(enum Parser_TypeSpec spec,
                                           const struct Lexer_Token *tok,
                                           struct DiagVec *diags)
{
    switch (spec) {
    case PARSER_TYPESPEC_INT:
        return PARSER_TYPESPEC_LONG;
    case PARSER_TYPESPEC_UINT:
        return PARSER_TYPESPEC_ULONG;
    case PARSER_TYPESPEC_DOUBLE:
        return PARSER_TYPESPEC_LONGDOUBLE;

    default:
        gen_dynpush(diags, bad_qual_err("long", tok));
        return spec;
    }
}

static enum Parser_TypeSpec make_spec_longlong(enum Parser_TypeSpec spec,
                                               const struct Lexer_Token *tok,
                                               struct DiagVec *diags)
{
    switch (spec) {
    case PARSER_TYPESPEC_INT:
        return PARSER_TYPESPEC_LONGLONG;
    case PARSER_TYPESPEC_UINT:
        return PARSER_TYPESPEC_ULONGLONG;

    default:
        gen_dynpush(diags, bad_qual_err("long long", tok));
        return spec;
    }
}

static void set_squal_flag(struct Parser_TypeStorQual *qual,
                           enum Lexer_TokenType type)
{
    switch (type) {
    case LEXER_TOKENTYPE_STATIC:
        qual->is_static = true;
        break;

    case LEXER_TOKENTYPE_CONSTEXPR:
        qual->is_constexpr = true;
        break;

    case LEXER_TOKENTYPE_TYPEDEF:
        qual->is_typedef = true;
        break;

    default:
        CRASH("token is not a storage qualifier");
    }
}

static void set_dqual_flag(struct Parser_TypeDataQual *qual,
                           enum Lexer_TokenType type)
{
    switch (type) {
    case LEXER_TOKENTYPE_CONST:
        qual->is_const = true;
        break;

    case LEXER_TOKENTYPE_VOLATILE:
        qual->is_volatile = true;
        break;

    default:
        CRASH("token is not a data qualifier");
    }
}

// parses the type specifier and its preceding qualifiers
// static const int *const &x
// ^^^^^^^^^^^^^^^^
struct Parser_Type Parser_parse_base(const struct Lexer_Token *toks,
                                     isize_t start, isize_t *out_end,
                                     struct Sema_Scope *scope,
                                     struct DiagVec *diags)
{
    struct Parser_Type ret = {};

    isize_t i = start;

    // this could definitely be written way better

    // point to the token holding the modifier
    const struct Lexer_Token *is_signed = NULL;
    const struct Lexer_Token *is_unsigned = NULL;
    const struct Lexer_Token *is_short = NULL;
    const struct Lexer_Token *is_long = NULL;
    const struct Lexer_Token *is_longlong = NULL;

    struct Parser_TypeDataQual dquals = {};
    struct Parser_TypeStorQual squals = {};

    bool spec_is_typedef = false;
    bool missing_spec = true;

    for (; Lexer_is_typequal(toks[i].type) || Lexer_is_typemod(toks[i].type) ||
           tok_is_type_spec(scope, &toks[i]) ||
           tok_is_namespace_name(scope, &toks[i]) ||
           toks[i].type == LEXER_TOKENTYPE_SCOPE_RES;
         ++i) {
        if (Lexer_is_typedataqual(toks[i].type)) {
            set_dqual_flag(&dquals, toks[i].type);
        } else if (toks[i].type == LEXER_TOKENTYPE_SIGNED) {
            if (is_signed)
                gen_dynpush(diags, unnecessary_qual_warn("signed", &toks[i]));
            if (is_unsigned)
                gen_dynpush(diags, bad_qual_err("signed", &toks[i]));
            is_signed = &toks[i];
        } else if (toks[i].type == LEXER_TOKENTYPE_UNSIGNED) {
            if (is_unsigned)
                gen_dynpush(diags, unnecessary_qual_warn("unsigned", &toks[i]));
            if (is_signed)
                gen_dynpush(diags, bad_qual_err("unsigned", &toks[i]));
            is_unsigned = &toks[i];
        } else if (toks[i].type == LEXER_TOKENTYPE_SHORT) {
            if (is_short)
                gen_dynpush(diags, unnecessary_qual_warn("short", &toks[i]));
            else if (is_long || is_longlong)
                gen_dynpush(diags, bad_qual_err("short", &toks[i]));
            is_short = &toks[i];
        } else if (toks[i].type == LEXER_TOKENTYPE_LONG) {
            if (is_longlong || is_short) {
                gen_dynpush(diags, bad_qual_err("long", &toks[i]));
            } else if (is_long) {
                is_longlong = &toks[i];
                is_long = NULL;
            } else {
                is_long = &toks[i];
            }
        } else if (Lexer_is_typequal(toks[i].type)) {
            set_squal_flag(&squals, toks[i].type);
        } else if (missing_spec) {
            missing_spec = false;
            auto res = Parser_parse_scope_res(toks, i, &i, scope, diags);
            ret = Sema_tok_type(res, &toks[i]);
            spec_is_typedef = ret.squals.is_typedef;
        } else {
            break;
        }
    }
    ret.squals = squals;
    if (!spec_is_typedef && !missing_spec)
        ret.dquals.arr[0] = dquals;
    else if (!spec_is_typedef && missing_spec)
        gen_dynpush(&ret.dquals, dquals);

    // short, long and long long don't need a type spec
    if ((is_short || is_long || is_longlong) && missing_spec) {
        missing_spec = false;
        ret.spec = PARSER_TYPESPEC_INT;
    } else if (missing_spec) {
        struct Diag err = {.pos = toks[start].pos,
                           .line = toks[start].line,
                           .msg = strdup("expected a type specifier"),
                           .err = ERRORTYPE_MISSING_TYPESPEC,
                           .is_err = true};
        gen_dynpush(diags, err);
        ret.spec = PARSER_TYPESPEC_INT; // default to int
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

static struct Parser_Type
parse_recursive_part(const struct Lexer_Token *toks, isize_t start, isize_t min,
                     isize_t *out_end, struct Sema_Scope *scope,
                     const struct Parser_TypeStorQual *squals,
                     struct DiagVec *diags);

// returns the end of the function ptr
// void (*func_ptr)(int, float)
//      ^         ^           ^
//    lparen    rparen      return
static isize_t parse_fptr(struct Parser_Type *type,
                          const struct Lexer_Token *toks, isize_t lparen,
                          isize_t rparen, isize_t min, struct Sema_Scope *scope,
                          struct DiagVec *diags)
{
    isize_t p_lparen = rparen + 1;
    isize_t p_rparen = Parser_find_twin_paren(toks, p_lparen, ISIZE_MAX);

    type->spec = PARSER_TYPESPEC_FPTR;
    type->fptr = mid_malloc(sizeof(*type->fptr));
    type->fptr->ret =
        parse_recursive_part(toks, lparen - 1, min, NULL, scope,
                             &(struct Parser_TypeStorQual){}, diags);
    type->fptr->params = (struct Parser_TypeVec){};

    isize_t i = p_lparen + 1;
    while (i < p_rparen) {
        gen_dynpush(&type->fptr->params,
                    Parser_parse_type(toks, i, &i, scope, NULL, diags));

        if (toks[i].type != LEXER_TOKENTYPE_COMMA &&
            toks[i].type != LEXER_TOKENTYPE_R_PAREN) {
            gen_dynpush(diags, expected_paren(false, toks));
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
static isize_t parse_array(struct Parser_Type *type,
                           const struct Lexer_Token *toks, isize_t lparen,
                           isize_t rparen, isize_t min,
                           struct Sema_Scope *scope, struct DiagVec *diags)
{
    // TODO: implement this
    CRASH("parse_array not implemented yet");
    (void)type;
    (void)toks;
    (void)lparen;
    (void)rparen;
    (void)min;
    (void)scope;
    (void)diags;
}

static struct Parser_Type
parse_recursive_part(const struct Lexer_Token *toks, isize_t start, isize_t min,
                     isize_t *out_end, struct Sema_Scope *scope,
                     const struct Parser_TypeStorQual *squals,
                     struct DiagVec *diags)
{
    struct Parser_Type ret = {.squals = *squals};

    struct Parser_TypeDataQual dquals = {};

    isize_t i;
    for (i = start;
         i >= min &&
         (Lexer_is_typedataqual(toks[i].type) || is_ptr_tok(toks[i].type) ||
          is_lv_ref_tok(toks[i].type) || is_rv_ref_tok(toks[i].type));
         --i) {
        if (Lexer_is_typedataqual(toks[i].type)) {
            set_dqual_flag(&dquals, toks[i].type);
        } else if (is_ptr_tok(toks[i].type)) {
            gen_dynpush(&ret.dquals, dquals);
            dquals = (struct Parser_TypeDataQual){};
        } else if (is_lv_ref_tok(toks[i].type)) {
            if (ret.lv_ref || ret.rv_ref)
                gen_dynpush(diags, type_alr_const_err(&toks[i]));
            else if (ret.dquals.len > 0)
                gen_dynpush(diags, ptr_to_ref_err(&toks[i]));
            else if (dquals.is_const)
                gen_dynpush(diags, missplaced_const_err(&toks[i]));
            else
                ret.lv_ref = true;
        } else {
            if (ret.lv_ref || ret.rv_ref)
                gen_dynpush(diags, type_alr_const_err(&toks[i]));
            else if (ret.dquals.len > 0)
                gen_dynpush(diags, ptr_to_ref_err(&toks[i]));
            else if (dquals.is_const)
                gen_dynpush(diags, missplaced_const_err(&toks[i]));
            else
                ret.rv_ref = true;
        }
    }

    // end is non inclusive
    isize_t end = start + 1;

    if (toks[i].type == LEXER_TOKENTYPE_L_PAREN) {
        isize_t rparen = Parser_find_twin_paren(toks, i, ISIZE_MAX);
        if (rparen == -1) {
            gen_dynpush(diags, expected_paren(false, &toks[i]));
        } else {
            if (toks[rparen + 1].type == LEXER_TOKENTYPE_L_PAREN)
                end = parse_fptr(&ret, toks, i, rparen, min, scope, diags);
            else if (toks[rparen + 1].type == LEXER_TOKENTYPE_L_SQBRACKET)
                end = parse_array(&ret, toks, i, rparen, min, scope, diags);
        }
    } else if (toks[end].type == LEXER_TOKENTYPE_IDENTIFIER) {
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
isize_t find_type_center(const struct Lexer_Token *toks, isize_t start)
{
    isize_t i = start;
    while (toks[i].type == LEXER_TOKENTYPE_L_PAREN ||
           Lexer_is_typedataqual(toks[i].type) || is_ptr_tok(toks[i].type) ||
           is_lv_ref_tok(toks[i].type) || is_rv_ref_tok(toks[i].type))
        ++i;

    if (toks[i].type == LEXER_TOKENTYPE_IDENTIFIER)
        ++i;

    return i - 1;
}

struct Parser_TypeFPtr Parser_copy_fptr_type(const struct Parser_TypeFPtr *fptr)
{
    struct Parser_TypeFPtr ret = {.has_ellipsis = fptr->has_ellipsis};
    ret.ret = Parser_copy_type(&fptr->ret);

    for (isize_t i = 0; i < fptr->params.len; ++i)
        gen_dynpush(&ret.params, Parser_copy_type(&fptr->params.arr[i]));

    return ret;
}

struct Parser_TypeArray
Parser_copy_array_type(const struct Parser_TypeArray *arr)
{
    struct Parser_TypeArray ret = {};
    ret.elem = Parser_copy_type(&arr->elem);
    ret.len = arr->len;
    return ret;
}

static void add_base(struct Parser_Type *type, const struct Parser_Type *base,
                     const struct Lexer_Token *type_start,
                     struct DiagVec *diags)
{
    if (type->spec == PARSER_TYPESPEC_FPTR) {
        add_base(&type->fptr->ret, base, type_start, diags);
    } else if (type->spec == PARSER_TYPESPEC_ARRAY) {
        add_base(&type->array->elem, base, type_start, diags);
    } else {
        if (base->spec == PARSER_TYPESPEC_FPTR) {
            type->fptr = mid_malloc(sizeof(*type->fptr));
            *type->fptr = Parser_copy_fptr_type(base->fptr);
        } else if (base->spec == PARSER_TYPESPEC_ARRAY) {
            type->array = mid_malloc(sizeof(*type->array));
            *type->array = Parser_copy_array_type(base->array);
        } else if (Parser_is_typespec_named(base->spec)) {
            type->named = base->named;
        } else if (base->spec == PARSER_TYPESPEC_FUNC) {
            type->func = base->func;
        }

        if (type->dquals.len > 0 && (base->lv_ref || base->rv_ref))
            gen_dynpush(diags, ptr_to_ref_err(type_start));

        for (isize_t i = 0; i < base->dquals.len; ++i)
            gen_dynpush(&type->dquals, base->dquals.arr[i]);
        type->spec = base->spec;
        type->squals = base->squals;
        type->lv_ref |= base->lv_ref;
        type->rv_ref |= base->rv_ref;
    }
}

struct Parser_Type Parser_parse_type(const struct Lexer_Token *toks,
                                     isize_t start, isize_t *out_end,
                                     struct Sema_Scope *scope,
                                     isize_t *out_declname,
                                     struct DiagVec *diags)
{
    isize_t i;
    auto base = Parser_parse_base(toks, start, &i, scope, diags);

    auto ret = Parser_parse_type_no_base(toks, i, out_end, &base, scope,
                                         out_declname, diags);

    Parser_Type_deinit(&base);
    return ret;
}

struct Parser_Type Parser_parse_type_no_base(const struct Lexer_Token *toks,
                                             isize_t start, isize_t *out_end,
                                             const struct Parser_Type *base,
                                             struct Sema_Scope *scope,
                                             isize_t *out_declname,
                                             struct DiagVec *diags)
{
    isize_t c = find_type_center(toks, start);

    bool has_declname = toks[c].type == LEXER_TOKENTYPE_IDENTIFIER &&
                        !Sema_is_type_name(scope, toks[c].ident);

    auto ret = parse_recursive_part(toks, c - has_declname, start, out_end,
                                    scope, &base->squals, diags);
    add_base(&ret, base, &toks[start], diags);

    if (out_declname)
        *out_declname = has_declname ? c : -1;
    return ret;
}

isize_t Parser_n_indir(const struct Parser_Type *type)
{
    return type->dquals.len - 1;
}

struct Parser_Type Parser_copy_type(const struct Parser_Type *type)
{
    struct Parser_Type ret = {
        .spec = type->spec,
        .squals = type->squals,
        .lv_ref = type->lv_ref,
        .rv_ref = type->rv_ref,
    };

    for (isize_t i = 0; i < type->dquals.len; ++i)
        gen_dynpush(&ret.dquals, type->dquals.arr[i]);

    if (type->spec == PARSER_TYPESPEC_FPTR) {
        ret.fptr = mid_malloc(sizeof(*ret.fptr));
        *ret.fptr = Parser_copy_fptr_type(type->fptr);
    } else if (type->spec == PARSER_TYPESPEC_ARRAY) {
        ret.array = mid_malloc(sizeof(*ret.array));
        *ret.array = Parser_copy_array_type(type->array);
    } else if (Parser_is_typespec_named(type->spec)) {
        ret.named = type->named;
    } else if (type->spec == PARSER_TYPESPEC_FUNC) {
        ret.func = type->func;
    }

    return ret;
}

struct Parser_Type Parser_ref_type(const struct Parser_Type *type,
                                   bool *out_failed)
{
    auto ret = Parser_copy_type(type);

    if (!ret.lv_ref && !ret.rv_ref) {
        gen_dynpush(&ret.dquals, (struct Parser_TypeDataQual){});
        if (out_failed)
            *out_failed = false;
    } else if (out_failed) {
        *out_failed = true;
    }

    return ret;
}

struct Parser_Type Parser_deref_type(const struct Parser_Type *type,
                                     bool *out_failed)
{
    auto ret = Parser_copy_type(type);

    if (ret.dquals.len > 1) {
        // the first element holds the top most ptr
        gen_dynremove(&ret.dquals, 0);
        if (out_failed)
            *out_failed = false;
    } else if (out_failed) {
        *out_failed = true;
    }

    return ret;
}

struct Parser_Type Parser_toktype_to_type(enum Lexer_TokenType type)
{
    struct Parser_Type ret = {};
    gen_dynpush(&ret.dquals, (struct Parser_TypeDataQual){});

    switch (type) {
    case LEXER_TOKENTYPE_VOID:
        ret.spec = PARSER_TYPESPEC_VOID;
        break;

    case LEXER_TOKENTYPE_CHAR:
        ret.spec = PARSER_TYPESPEC_CHAR;
        break;

    case LEXER_TOKENTYPE_WCHAR:
        ret.spec = PARSER_TYPESPEC_WCHAR;
        break;

    case LEXER_TOKENTYPE_CHAR16:
        ret.spec = PARSER_TYPESPEC_CHAR16;
        break;

    case LEXER_TOKENTYPE_CHAR32:
        ret.spec = PARSER_TYPESPEC_CHAR32;
        break;

    case LEXER_TOKENTYPE_INT:
        ret.spec = PARSER_TYPESPEC_INT;
        break;

    case LEXER_TOKENTYPE_FLOAT:
        ret.spec = PARSER_TYPESPEC_FLOAT;
        break;

    case LEXER_TOKENTYPE_DOUBLE:
        ret.spec = PARSER_TYPESPEC_DOUBLE;
        break;

    case LEXER_TOKENTYPE_BOOL:
        ret.spec = PARSER_TYPESPEC_BOOL;
        break;

    case LEXER_TOKENTYPE_STRUCT:
    case LEXER_TOKENTYPE_CLASS:
        ret.spec = PARSER_TYPESPEC_CLASS;
        break;

    case LEXER_TOKENTYPE_UNION:
        ret.spec = PARSER_TYPESPEC_UNION;
        break;

    case LEXER_TOKENTYPE_ENUM:
        ret.spec = PARSER_TYPESPEC_ENUM;
        break;

    case LEXER_TOKENTYPE_AUTO:
        ret.spec = PARSER_TYPESPEC_AUTO;
        break;

    default:
        CRASH("can only convert POD type spec tokens to Parser_Type");
    }

    return ret;
}

static void type_to_str_impl(const struct Parser_Type *type,
                             struct Dynstr *str);

static void fptr_to_str(const struct Parser_Type *type, struct Dynstr *str)
{
    type_to_str_impl(&type->fptr->ret, str);
    Dynstr_append_char(str, ' ');

    Dynstr_append_char(str, '(');
    for (isize_t i = 0; i < Parser_n_indir(type) + 1; ++i)
        Dynstr_append_char(str, '*');
    if (type->lv_ref)
        Dynstr_append_char(str, '&');
    else if (type->rv_ref)
        Dynstr_append(str, "&&");
    Dynstr_append_char(str, ')');

    Dynstr_append_char(str, '(');
    for (isize_t i = 0; i < type->fptr->params.len; ++i) {
        if (i > 0)
            Dynstr_append(str, ", ");
        type_to_str_impl(&type->fptr->params.arr[i], str);
    }
    Dynstr_append_char(str, ')');
}

static void array_to_str(const struct Parser_Type *type, struct Dynstr *str)
{
    type_to_str_impl(&type->array->elem, str);
    Dynstr_append_printf(str, "[%" PRIu64 "]", type->array->len);
}

static void dquals_to_str(const struct Parser_TypeDataQual *dquals,
                          struct Dynstr *str, bool leading_space,
                          bool trailing_space)
{
    if (dquals->is_const) {
        if (leading_space)
            Dynstr_append_char(str, ' ');
        Dynstr_append(str, "const");
        if (trailing_space)
            Dynstr_append_char(str, ' ');
    }
}

static void regular_type_to_str(const struct Parser_Type *type,
                                struct Dynstr *str)
{
    dquals_to_str(&type->dquals.arr[type->dquals.len - 1], str, false, true);
    Dynstr_append(str, Parser_typespec_to_str(type->spec));
    if (Parser_is_typespec_named(type->spec))
        Dynstr_append_printf(
            str, " %s", type->named.parent->idents.arr[type->named.ident].name);

    for (isize_t i = Parser_n_indir(type); i > 0; --i) {
        Dynstr_append_char(str, '*');
        dquals_to_str(&type->dquals.arr[i - 1], str, true, false);
    }

    if (type->lv_ref)
        Dynstr_append_char(str, '&');
    else if (type->rv_ref)
        Dynstr_append(str, "&&");
}

static void type_to_str_impl(const struct Parser_Type *type, struct Dynstr *str)
{
    if (type->spec == PARSER_TYPESPEC_FPTR)
        fptr_to_str(type, str);
    else if (type->spec == PARSER_TYPESPEC_ARRAY)
        array_to_str(type, str);
    else if (type->spec == PARSER_TYPESPEC_INVALID)
        Dynstr_append(str, "INVALID-TYPE");
    else
        regular_type_to_str(type, str);
}

char *Parser_type_to_str(const struct Parser_Type *type)
{
    struct Dynstr str = Dynstr();
    type_to_str_impl(type, &str);
    return str.str;
}

bool Parser_valid_type_start(const struct Lexer_Token *toks, isize_t idx,
                             const struct Sema_Scope *scope)
{
    if (Lexer_is_typemod(toks[idx].type) || Lexer_is_typequal(toks[idx].type))
        return true;

    struct DiagVec tmp = {};
    isize_t res_end;
    auto res = Parser_parse_scope_res_const(toks, idx, &res_end, scope, &tmp);
    gen_dyndeinit(&tmp);

    return tok_is_type_spec(res, &toks[res_end]);
}

enum Parser_TypeSpec Parser_uint_type_of_width(i32 bytes)
{
    if (Types_char_size == bytes)
        return PARSER_TYPESPEC_UCHAR;
    else if (Types_short_size == bytes)
        return PARSER_TYPESPEC_USHORT;
    else if (Types_int_size == bytes)
        return PARSER_TYPESPEC_UINT;
    else if (Types_long_size == bytes)
        return PARSER_TYPESPEC_ULONG;
    else if (Types_longlong_size == bytes)
        return PARSER_TYPESPEC_ULONGLONG;
    else
        return PARSER_TYPESPEC_INVALID;
}

enum Parser_TypeSpec Parser_sint_type_of_width(i32 bytes)
{
    if (Types_char_size == bytes)
        return PARSER_TYPESPEC_SCHAR;
    else if (Types_short_size == bytes)
        return PARSER_TYPESPEC_SHORT;
    else if (Types_int_size == bytes)
        return PARSER_TYPESPEC_INT;
    else if (Types_long_size == bytes)
        return PARSER_TYPESPEC_LONG;
    else if (Types_longlong_size == bytes)
        return PARSER_TYPESPEC_LONGLONG;
    else
        return PARSER_TYPESPEC_INVALID;
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

i32 Parser_typespec_conv_rank(enum Parser_TypeSpec spec)
{
    switch (spec) {
    case PARSER_TYPESPEC_BOOL:
        return 10;

    case PARSER_TYPESPEC_CHAR:
    case PARSER_TYPESPEC_SCHAR:
    case PARSER_TYPESPEC_UCHAR:
        return 20;

    case PARSER_TYPESPEC_SHORT:
    case PARSER_TYPESPEC_USHORT:
        return 30;

    case PARSER_TYPESPEC_INT:
    case PARSER_TYPESPEC_UINT:
        return 40;

    case PARSER_TYPESPEC_LONG:
    case PARSER_TYPESPEC_ULONG:
        return 50;

    case PARSER_TYPESPEC_LONGLONG:
    case PARSER_TYPESPEC_ULONGLONG:
        return 60;

    case PARSER_TYPESPEC_FLOAT:
        return 70;

    case PARSER_TYPESPEC_DOUBLE:
        return 80;

    case PARSER_TYPESPEC_LONGDOUBLE:
        return 90;

    case PARSER_TYPESPEC_WCHAR:
        if (Types_wchar_signed)
            return Parser_typespec_conv_rank(
                Parser_sint_type_of_width(Types_wchar_size));
        else
            return Parser_typespec_conv_rank(
                Parser_uint_type_of_width(Types_wchar_size));
    case PARSER_TYPESPEC_CHAR16:
        return Parser_typespec_conv_rank(Parser_uint_type_of_width(16 / 8));
    case PARSER_TYPESPEC_CHAR32:
        return Parser_typespec_conv_rank(Parser_uint_type_of_width(32 / 8));

    default:
        printf("type = %d\n", spec);
        CRASH("type doesn't have a rank");
    }
}

u64 Parser_integral_max(enum Parser_TypeSpec spec)
{
    switch (spec) {
    case PARSER_TYPESPEC_CHAR:
        return Types_char_signed ? Types_char_smax : Types_char_umax;
    case PARSER_TYPESPEC_SCHAR:
        return Types_char_smax;
    case PARSER_TYPESPEC_UCHAR:
        return Types_char_umax;
    case PARSER_TYPESPEC_WCHAR:
        if (Types_wchar_signed)
            return Parser_integral_max(
                Parser_sint_type_of_width(Types_wchar_size));
        else
            return Parser_integral_max(
                Parser_uint_type_of_width(Types_wchar_size));
    case PARSER_TYPESPEC_CHAR16:
        return Parser_integral_max(Parser_uint_type_of_width(16 / 8));
    case PARSER_TYPESPEC_CHAR32:
        return Parser_integral_max(Parser_uint_type_of_width(32 / 8));

    case PARSER_TYPESPEC_SHORT:
        return Types_short_smax;
    case PARSER_TYPESPEC_USHORT:
        return Types_short_umax;

    case PARSER_TYPESPEC_INT:
        return Types_int_smax;
    case PARSER_TYPESPEC_UINT:
        return Types_int_umax;

    case PARSER_TYPESPEC_LONG:
        return Types_long_smax;
    case PARSER_TYPESPEC_ULONG:
        return Types_long_umax;

    case PARSER_TYPESPEC_LONGLONG:
        return Types_longlong_smax;
    case PARSER_TYPESPEC_ULONGLONG:
        return Types_longlong_umax;

    case PARSER_TYPESPEC_BOOL:
        return 1;

    default:
        assert(!Parser_is_integral_typespec(spec));
        CRASH("spec isn't integral");
    }
}

i64 Parser_integral_min(enum Parser_TypeSpec spec)
{
    switch (spec) {
    case PARSER_TYPESPEC_CHAR:
        return Types_char_signed ? Types_char_smin : 0;
    case PARSER_TYPESPEC_SCHAR:
        return Types_char_smin;
    case PARSER_TYPESPEC_UCHAR:
        return 0;
    case PARSER_TYPESPEC_WCHAR:
        if (Types_wchar_signed)
            return Parser_integral_min(
                Parser_sint_type_of_width(Types_wchar_size));
        else
            return 0;
    case PARSER_TYPESPEC_CHAR16:
        return 0;
    case PARSER_TYPESPEC_CHAR32:
        return 0;

    case PARSER_TYPESPEC_SHORT:
        return Types_short_smin;
    case PARSER_TYPESPEC_USHORT:
        return 0;

    case PARSER_TYPESPEC_INT:
        return Types_int_smin;
    case PARSER_TYPESPEC_UINT:
        return 0;

    case PARSER_TYPESPEC_LONG:
        return Types_long_smin;
    case PARSER_TYPESPEC_ULONG:
        return 0;

    case PARSER_TYPESPEC_LONGLONG:
        return Types_longlong_smin;
    case PARSER_TYPESPEC_ULONGLONG:
        return 0;

    case PARSER_TYPESPEC_BOOL:
        return 0;

    default:
        assert(!Parser_is_integral_typespec(spec));
        CRASH("spec isn't integral");
    }
}

enum Parser_TypeSpec Parser_integral_prom(enum Parser_TypeSpec spec)
{
    assert(Parser_is_integral_typespec(spec));

    if (spec == PARSER_TYPESPEC_BOOL)
        return PARSER_TYPESPEC_INT;

    i32 spec_rank = Parser_typespec_conv_rank(spec);
    i32 int_rank = Parser_typespec_conv_rank(PARSER_TYPESPEC_INT);

    if (spec_rank < int_rank) {
        if (Parser_integral_max(PARSER_TYPESPEC_INT) >=
                Parser_integral_max(spec) &&
            Parser_integral_min(PARSER_TYPESPEC_INT) <=
                Parser_integral_min(spec))
            return PARSER_TYPESPEC_INT;
        else
            return PARSER_TYPESPEC_UINT;
    } else {
        return spec;
    }
}

bool Parser_is_fundamental_type(const struct Parser_Type *type)
{
    return Parser_n_indir(type) == 0 &&
           (Parser_is_integral_typespec(type->spec) ||
            Parser_is_floating_typespec(type->spec));
}

static bool are_fptrs_same(const struct Parser_TypeFPtr *a,
                           const struct Parser_TypeFPtr *b)
{
    if (a->params.len != b->params.len)
        return false;
    else if (a->has_ellipsis != b->has_ellipsis)
        return false;
    else if (!Parser_are_types_same(&a->ret, &b->ret))
        return false;

    for (isize_t i = 0; i < a->params.len; ++i) {
        if (!Parser_are_types_same(&a->params.arr[i], &b->params.arr[i]))
            return false;
    }

    return true;
}

static bool are_arrays_same(const struct Parser_TypeArray *a,
                            const struct Parser_TypeArray *b)
{
    if (a->len != b->len)
        return false;

    return Parser_are_types_same(&a->elem, &b->elem);
}

bool Parser_dquals_same(const struct Parser_TypeDataQual *a, isize_t n_a,
                        const struct Parser_TypeDataQual *b, isize_t n_b)
{
    if (n_a != n_b)
        return false;

    for (isize_t i = 0; i < n_a; ++i) {
        if (a[i].is_const != b[i].is_const ||
            a[i].is_volatile != b[i].is_volatile)
            return false;
    }

    return true;
}

bool Parser_squals_same(const struct Parser_TypeStorQual *a,
                        const struct Parser_TypeStorQual *b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

bool Parser_are_types_same(const struct Parser_Type *a,
                           const struct Parser_Type *b)
{
    if (a->spec != b->spec)
        return false;
    else if (a->lv_ref != b->lv_ref || a->rv_ref != b->rv_ref)
        return false;
    else if (!Parser_squals_same(&a->squals, &b->squals))
        return false;
    else if (!Parser_dquals_same(a->dquals.arr, a->dquals.len, b->dquals.arr,
                                 b->dquals.len))
        return false;
    else if (a->spec == PARSER_TYPESPEC_FPTR)
        return are_fptrs_same(a->fptr, b->fptr);
    else if (a->spec == PARSER_TYPESPEC_ARRAY)
        return are_arrays_same(a->array, b->array);
    else if (Parser_is_typespec_named(a->spec))
        return a->named.parent == b->named.parent &&
               a->named.ident == b->named.ident;
    else
        return true;
}

struct Sema_Ident *Parser_named_type_ident(const struct Parser_TypeNamed *named)
{
    assert(named->ident != -1);
    return &named->parent->idents.arr[named->ident];
}

struct Parser_Type Parser_create_func_type(struct Sema_Scope *scope,
                                           const char *name)
{
    struct Parser_Type ret = {};
    ret.spec = PARSER_TYPESPEC_FUNC;
    ret.func.scope = scope;
    ret.func.name = name;

    gen_dynpush(&ret.dquals, ((struct Parser_TypeDataQual){}));

    return ret;
}
