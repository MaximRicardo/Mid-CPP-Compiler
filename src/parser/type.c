#include "type.h"
#include "diag.h"
#include "generics/dynarray.h"
#include "ints.h"
#include "lexer/token.h"
#include "parser/find_twin.h"
#include "print.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool Parser_is_typespec_named(enum Parser_TypeSpec spec)
{
    return spec == PARSER_TYPESPEC_STRUCT || spec == PARSER_TYPESPEC_CLASS ||
           spec == PARSER_TYPESPEC_ENUM || spec == PARSER_TYPESPEC_ENUMCLASS ||
           spec == PARSER_TYPESPEC_UNION;
}

enum Parser_TypeSpec Parser_toktype_to_typespec(enum Lexer_TokenType type)
{
    switch (type) {
    case LEXER_TOKENTYPE_CHAR:
        return PARSER_TYPESPEC_CHAR;

    case LEXER_TOKENTYPE_INT:
        return PARSER_TYPESPEC_INT;

    case LEXER_TOKENTYPE_FLOAT:
        return PARSER_TYPESPEC_FLOAT;

    case LEXER_TOKENTYPE_DOUBLE:
        return PARSER_TYPESPEC_DOUBLE;

    default:
        assert(false);
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

    default:
        assert(false);
    }
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

const char *spec_to_str(enum Parser_TypeSpec spec)
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

    case PARSER_TYPESPEC_AUTO:
        return "auto";

    case PARSER_TYPESPEC_CLASS:
        return "class";
    case PARSER_TYPESPEC_STRUCT:
        return "struct";
    case PARSER_TYPESPEC_UNION:
        return "union";
    case PARSER_TYPESPEC_ENUM:
        return "enum";
    case PARSER_TYPESPEC_ENUMCLASS:
        return "enum class";

    case PARSER_TYPESPEC_FPTR:
    case PARSER_TYPESPEC_ARRAY:
        assert(false);
        return "INVALID-TYPE";
    }
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
        gen_dynpush(diags, spec_unsignable_err(spec_to_str(spec), tok));
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
        gen_dynpush(diags, spec_unsignable_err(spec_to_str(spec), tok));
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

// parses the type specifier and its preceding qualifiers
// static const int *const &x
// ^^^^^^^^^^^^^^^^
struct Parser_Type parse_typespec(const struct Lexer_Token *toks, isize_t start,
                                  isize_t *out_end, struct DiagVec *diags)
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

    bool missing_spec = true;

    for (; Lexer_is_typequal(toks[i].type) || Lexer_is_typespec(toks[i].type) ||
           Lexer_is_typemod(toks[i].type);
         ++i) {
        if (toks[i].type == LEXER_TOKENTYPE_CONST) {
            if (dquals.is_const)
                gen_dynpush(diags, unnecessary_qual_warn("const", &toks[i]));
            dquals.is_const = &toks[i];
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
            set_squal_flag(&ret.squals, toks[i].type);
        } else {
            missing_spec = false;
            ret.spec = Parser_toktype_to_typespec(toks[i].type);
        }
    }
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

// start is the idx of the left paren starting the parameter list
// returns the end of the function ptr
// void (*ptr)(int, float)
//            ^          ^
//          start       end
static isize_t parse_fptr(struct Parser_Type *type,
                          const struct Lexer_Token *toks, isize_t start,
                          struct DiagVec *diags)
{
    isize_t rparen = Parser_find_twin_paren(toks, start, ISIZE_MAX);

    type->spec = PARSER_TYPESPEC_FPTR;
    type->fptr = malloc(sizeof(*type->fptr));
    type->fptr->ret = (struct Parser_Type){};
    type->fptr->params = (struct Parser_TypeVec){};

    isize_t i = start + 1;
    while (i < rparen) {
        gen_dynpush(&type->fptr->params,
                    Parser_parse_type(toks, i, &i, NULL, diags));

        if (toks[i].type != LEXER_TOKENTYPE_COMMA &&
            toks[i].type != LEXER_TOKENTYPE_R_PAREN) {
            gen_dynpush(diags, expected_paren(false, toks));
        }

        ++i;
    }

    return rparen + 1;
}

// start is the idx of the left bracket
// returns the end of the array
// int arr[height][width]
//        ^             ^
//      start          end
static isize_t parse_array(struct Parser_Type *type,
                           const struct Lexer_Token *toks, isize_t start,
                           struct DiagVec *diags)
{
    // TODO: implement this
    assert(false);
    (void)type;
    (void)toks;
    (void)start;
    (void)diags;
}

struct Parser_Type parse_other_part(const struct Lexer_Token *toks,
                                    isize_t start, isize_t min,
                                    isize_t *out_end,
                                    const struct Parser_TypeStorQual *squals,
                                    struct DiagVec *diags)
{
    struct Parser_Type ret = {.squals = *squals};

    struct Parser_TypeDataQual dquals = {};

    isize_t i;
    for (i = start;
         i >= min &&
         (toks[i].type == LEXER_TOKENTYPE_CONST || is_ptr_tok(toks[i].type) ||
          is_lv_ref_tok(toks[i].type) || is_rv_ref_tok(toks[i].type));
         --i) {
        if (toks[i].type == LEXER_TOKENTYPE_CONST) {
            if (dquals.is_const)
                gen_dynpush(diags, unnecessary_qual_warn("const", &toks[i]));
            dquals.is_const = true;
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
                end = parse_fptr(&ret, toks, rparen + 1, diags);
            else if (toks[rparen + 1].type == LEXER_TOKENTYPE_L_SQBRACKET)
                end = parse_array(&ret, toks, rparen + 1, diags);
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
           toks[i].type == LEXER_TOKENTYPE_CONST || is_ptr_tok(toks[i].type) ||
           is_lv_ref_tok(toks[i].type) || is_rv_ref_tok(toks[i].type) ||
           toks[i].type == LEXER_TOKENTYPE_IDENTIFIER)
        ++i;

    return i - 1;
}

static void add_base(struct Parser_Type *type, const struct Parser_Type *base)
{
    if (type->spec == PARSER_TYPESPEC_FPTR) {
        add_base(&type->fptr->ret, base);
    } else if (type->spec == PARSER_TYPESPEC_ARRAY) {
        add_base(&type->array->elem, base);
    } else {
        gen_dynpush(&type->dquals, base->dquals.arr[0]);
        type->spec = base->spec;
        type->squals = base->squals;
    }
}

struct Parser_Type Parser_parse_type(const struct Lexer_Token *toks,
                                     isize_t start, isize_t *out_end,
                                     const char **out_declname,
                                     struct DiagVec *diags)
{
    isize_t i;
    auto base = parse_typespec(toks, start, &i, diags);

    printf("i = %" PRIisz "\n", i);
    isize_t c = find_type_center(toks, i);

    const char *declname =
        toks[c].type == LEXER_TOKENTYPE_IDENTIFIER ? toks[c].ident : NULL;

    auto ret = parse_other_part(toks, c - (declname != NULL), i, out_end,
                                &base.squals, diags);
    add_base(&ret, &base);

    if (out_declname)
        *out_declname = declname;
    Parser_Type_deinit(&base);
    return ret;
}

isize_t Parser_n_indir(const struct Parser_Type *type)
{
    return type->dquals.len - 1;
}
