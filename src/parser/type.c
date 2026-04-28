#include "type.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "macros.h"
#include "print.h"
#include "vecs.h"
#include <assert.h>
#include <string.h>

enum Parser_TypeSpec Parser_toktype_to_typespec(enum Lexer_TokenType type)
{
    switch (type) {
    case LEXER_TOKENTYPE_CHAR_SPEC:
        return PARSER_TYPESPEC_CHAR;

    case LEXER_TOKENTYPE_SCHAR_SPEC:
        return PARSER_TYPESPEC_SCHAR;

    case LEXER_TOKENTYPE_UCHAR_SPEC:
        return PARSER_TYPESPEC_UCHAR;

    case LEXER_TOKENTYPE_SHORT_SPEC:
        return PARSER_TYPESPEC_SHORT;

    case LEXER_TOKENTYPE_USHORT_SPEC:
        return PARSER_TYPESPEC_USHORT;

    case LEXER_TOKENTYPE_INT_SPEC:
        return PARSER_TYPESPEC_INT;

    case LEXER_TOKENTYPE_UINT_SPEC:
        return PARSER_TYPESPEC_UINT;

    case LEXER_TOKENTYPE_LONG_SPEC:
        return PARSER_TYPESPEC_LONG;

    case LEXER_TOKENTYPE_ULONG_SPEC:
        return PARSER_TYPESPEC_ULONG;

    case LEXER_TOKENTYPE_LONGLONG_SPEC:
        return PARSER_TYPESPEC_LONGLONG;

    case LEXER_TOKENTYPE_ULONGLONG_SPEC:
        return PARSER_TYPESPEC_ULONGLONG;

    case LEXER_TOKENTYPE_FLOAT_SPEC:
        return PARSER_TYPESPEC_FLOAT;

    case LEXER_TOKENTYPE_DOUBLE_SPEC:
        return PARSER_TYPESPEC_DOUBLE;

    case LEXER_TOKENTYPE_LONGDOUBLE_SPEC:
        return PARSER_TYPESPEC_LONGDOUBLE;

    default:
        assert(false);
    }
}

static void set_qual_flag(struct Parser_TypeQual *qual,
                          enum Lexer_TokenType type)
{
    switch (type) {
    case LEXER_TOKENTYPE_STATIC:
        qual->is_static = true;
        break;

    case LEXER_TOKENTYPE_CONSTEXPR:
        qual->is_constexpr = true;
        break;

    default:
        assert(false);
    }
}

void Parser_Type_deinit(struct Parser_Type *self)
{
    gen_dyndeinit(&self->is_const);
}

bool is_ptr_tok(enum Lexer_TokenType type)
{
    return type == LEXER_TOKENTYPE_MUL || type == LEXER_TOKENTYPE_DEREF;
}

bool is_lv_ref_tok(enum Lexer_TokenType type)
{
    return type == LEXER_TOKENTYPE_BITWISE_AND || type == LEXER_TOKENTYPE_REF;
}

bool is_rv_ref_tok(enum Lexer_TokenType type)
{
    return type == LEXER_TOKENTYPE_LOGICAL_AND;
}

static struct Diag unnecessary_const_warn(const struct Lexer_Token *tok)
{
    return (struct Diag){
        .pos = tok->pos,
        .line = tok->line,
        .msg = Print_fmt_to_str("unnecessary const specifier"),
        .warn = WARNTYPE_UNNECESSARY_CONST,
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

static void flip_boolarr(const struct BoolVec *arr)
{
    for (isize_t i = 0; i < arr->len / 2; ++i) {
        isize_t j = arr->len - i - 1;
        SWAP(arr->arr[i], arr->arr[j]);
    }
}

struct Parser_Type Parser_parse_type(const struct Lexer_Token *toks,
                                     isize_t start, isize_t *end,
                                     struct DiagVec *diags)
{
    struct Parser_Type ret = {.quals = gen_dyninit(),
                              .is_const = gen_dyninit()};

    isize_t i = start;

    bool is_const = false;

    for (; Lexer_is_typequal(toks[i].type); ++i) {
        if (toks[i].type == LEXER_TOKENTYPE_CONST) {
            if (is_const)
                gen_dynpush(diags, unnecessary_const_warn(&toks[i]));
            is_const = true;
        } else {
            set_qual_flag(&ret.quals, toks[i].type);
        }
    }

    if (!Lexer_is_typespec(toks[i].type)) {
        struct Diag err = {.pos = toks[start].pos,
                           .line = toks[start].line,
                           .msg = strdup("expected a type specifier"),
                           .err = ERRORTYPE_MISSING_TYPESPEC,
                           .is_err = true};
        gen_dynpush(diags, err);
    } else {
        ret.spec = Parser_toktype_to_typespec(toks[i++].type);
    }

    for (; toks[i].type == LEXER_TOKENTYPE_CONST || is_ptr_tok(toks[i].type) ||
           is_lv_ref_tok(toks[i].type) || is_rv_ref_tok(toks[i].type);
         ++i) {
        if (toks[i].type == LEXER_TOKENTYPE_CONST) {
            if (is_const)
                gen_dynpush(diags, unnecessary_const_warn(&toks[i]));
            if (ret.is_lv_ref || ret.is_rv_ref)
                gen_dynpush(diags, missplaced_const_err(&toks[i]));
            is_const = true;
        } else if (is_ptr_tok(toks[i].type)) {
            if (ret.is_lv_ref || ret.is_rv_ref)
                gen_dynpush(diags, ptr_to_ref_err(&toks[i]));
            gen_dynpush(&ret.is_const, is_const);
            is_const = false;
        } else if (is_lv_ref_tok(toks[i].type)) {
            if (ret.is_lv_ref || ret.is_rv_ref)
                gen_dynpush(diags, type_alr_const_err(&toks[i]));
            else
                ret.is_lv_ref = true;
        } else {
            if (ret.is_lv_ref || ret.is_rv_ref)
                gen_dynpush(diags, type_alr_const_err(&toks[i]));
            else
                ret.is_rv_ref = true;
        }
    }
    gen_dynpush(&ret.is_const, is_const);

    if (ret.quals.is_constexpr)
        ret.is_const.arr[0] = true;
    // is_const needs to go from most to least indirection
    flip_boolarr(&ret.is_const);

    if (end)
        *end = i;
    return ret;
}

isize_t Parser_n_indir(const struct Parser_Type *type)
{
    return type->is_const.len - 1;
}
