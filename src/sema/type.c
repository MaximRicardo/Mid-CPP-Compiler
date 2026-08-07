#include "sema/type.h"
#include "dynstr.h"
#include "macros.h"
#include "parser/ast.h"
#include "sema/class.h"
#include "sema/scope.h"

bool midsema_is_typespec_typecheckable(enum midpar_TypeSpec spec)
{
    return spec != MIDPAR_TYPESPEC_TEMPLATED && spec != MIDPAR_TYPESPEC_UNKNOWN;
}

bool midsema_is_typespec_named(enum midpar_TypeSpec spec)
{
    return spec == MIDPAR_TYPESPEC_CLASS || spec == MIDPAR_TYPESPEC_ENUM ||
           spec == MIDPAR_TYPESPEC_UNION || spec == MIDPAR_TYPESPEC_TEMPLATED;
}

enum midpar_TypeSpec midsema_toktype_to_typespec(enum midlex_TokenType type)
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

const char *midsema_typespec_to_str(enum midpar_TypeSpec spec)
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

bool midsema_is_integral_typespec(enum midpar_TypeSpec spec)
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

bool midsema_is_signed_integral_typespec(enum midpar_TypeSpec spec)
{
    return (spec == MIDPAR_TYPESPEC_CHAR && midtype_char_signed) ||
           spec == MIDPAR_TYPESPEC_SCHAR ||
           (spec == MIDPAR_TYPESPEC_WCHAR && midtype_wchar_signed) ||
           spec == MIDPAR_TYPESPEC_SHORT || spec == MIDPAR_TYPESPEC_INT ||
           spec == MIDPAR_TYPESPEC_LONG || spec == MIDPAR_TYPESPEC_LONGLONG ||
           spec == MIDPAR_TYPESPEC_BOOL;
}

bool midsema_is_unsigned_integral_typespec(enum midpar_TypeSpec spec)
{
    return midsema_is_integral_typespec(spec) &&
           !midsema_is_signed_integral_typespec(spec);
}

bool midsema_is_floating_typespec(enum midpar_TypeSpec spec)
{
    return spec == MIDPAR_TYPESPEC_FLOAT || spec == MIDPAR_TYPESPEC_DOUBLE ||
           spec == MIDPAR_TYPESPEC_LONGDOUBLE;
}

mid_isize midsema_n_indir(const struct midpar_Type *type)
{
    return type->dquals.len - 1;
}

struct midpar_Type midsema_ref_type(const struct midpar_Type *type,
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

struct midpar_Type midsema_deref_type(const struct midpar_Type *type,
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

static void type_to_str_impl(const struct midpar_Type *type,
                             struct mid_Dynstr *str);

static void fptr_to_str(const struct midpar_Type *type, struct mid_Dynstr *str)
{
    type_to_str_impl(&type->fptr->ret, str);
    midstr_append_char(str, ' ');

    midstr_append_char(str, '(');
    for (mid_isize i = 0; i < midsema_n_indir(type) + 1; ++i)
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
    midstr_append(str, midsema_typespec_to_str(type->spec));
    if (midsema_is_typespec_named(type->spec))
        midstr_append_printf(
            str, " %s", type->named.parent->idents.arr[type->named.idx].name);

    for (mid_isize i = midsema_n_indir(type); i > 0; --i) {
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

char *midsema_type_to_str(const struct midpar_Type *type)
{
    struct mid_Dynstr str = midstr_init();
    type_to_str_impl(type, &str);
    return str.str;
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
int32_t midsema_typespec_conv_rank(enum midpar_TypeSpec spec)
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
            return midsema_typespec_conv_rank(
                midpar_sint_type_of_width(midtype_wchar_size));
        else
            return midsema_typespec_conv_rank(
                midpar_uint_type_of_width(midtype_wchar_size));
    case MIDPAR_TYPESPEC_CHAR16:
        return midsema_typespec_conv_rank(midpar_uint_type_of_width(16 / 8));
    case MIDPAR_TYPESPEC_CHAR32:
        return midsema_typespec_conv_rank(midpar_uint_type_of_width(32 / 8));

    default:
        MID_CRASH("type doesn't have a rank");
    }
}

struct mid_APInt midsema_integral_max(enum midpar_TypeSpec spec)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_CHAR:
        return midint_copy(midtype_char_signed ? midtype_char_smax()
                                               : midtype_char_umax());
    case MIDPAR_TYPESPEC_SCHAR:
        return midint_copy(midtype_char_smax());
    case MIDPAR_TYPESPEC_UCHAR:
        return midint_copy(midtype_char_umax());
    case MIDPAR_TYPESPEC_WCHAR:
        if (midtype_wchar_signed)
            return midsema_integral_max(
                midpar_sint_type_of_width(midtype_wchar_size));
        else
            return midsema_integral_max(
                midpar_uint_type_of_width(midtype_wchar_size));
    case MIDPAR_TYPESPEC_CHAR16:
        return midsema_integral_max(midpar_uint_type_of_width(16 / 8));
    case MIDPAR_TYPESPEC_CHAR32:
        return midsema_integral_max(midpar_uint_type_of_width(32 / 8));

    case MIDPAR_TYPESPEC_SHORT:
        return midint_copy(midtype_short_smax());
    case MIDPAR_TYPESPEC_USHORT:
        return midint_copy(midtype_short_umax());

    case MIDPAR_TYPESPEC_INT:
        return midint_copy(midtype_int_smax());
    case MIDPAR_TYPESPEC_UINT:
        return midint_copy(midtype_int_umax());

    case MIDPAR_TYPESPEC_LONG:
        return midint_copy(midtype_long_smax());
    case MIDPAR_TYPESPEC_ULONG:
        return midint_copy(midtype_long_umax());

    case MIDPAR_TYPESPEC_LONGLONG:
        return midint_copy(midtype_longlong_smax());
    case MIDPAR_TYPESPEC_ULONGLONG:
        return midint_copy(midtype_longlong_umax());

    case MIDPAR_TYPESPEC_BOOL:
        return midint_one(midtype_bool_size * 8);

    default:
        assert(!midsema_is_integral_typespec(spec));
        MID_CRASH("spec isn't integral");
    }
}

struct mid_APInt midsema_integral_min(enum midpar_TypeSpec spec)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_CHAR:
        if (midtype_char_signed)
            return midint_copy(midtype_char_smin());
        else
            return midint_zero(midtype_char_size * 8);
    case MIDPAR_TYPESPEC_SCHAR:
        return midint_copy(midtype_char_smin());
    case MIDPAR_TYPESPEC_UCHAR:
        return midint_zero(midtype_char_size * 8);
    case MIDPAR_TYPESPEC_WCHAR:
        if (midtype_wchar_signed)
            return midsema_integral_min(
                midpar_sint_type_of_width(midtype_wchar_size));
        else
            return midint_zero(midtype_wchar_size * 8);
    case MIDPAR_TYPESPEC_CHAR16:
        return midint_zero(16);
    case MIDPAR_TYPESPEC_CHAR32:
        return midint_zero(32);

    case MIDPAR_TYPESPEC_SHORT:
        return midint_copy(midtype_short_smin());
    case MIDPAR_TYPESPEC_USHORT:
        return midint_zero(midtype_short_size * 8);

    case MIDPAR_TYPESPEC_INT:
        return midint_copy(midtype_int_smin());
    case MIDPAR_TYPESPEC_UINT:
        return midint_zero(midtype_int_size * 8);

    case MIDPAR_TYPESPEC_LONG:
        return midint_copy(midtype_long_smin());
    case MIDPAR_TYPESPEC_ULONG:
        return midint_zero(midtype_long_size * 8);

    case MIDPAR_TYPESPEC_LONGLONG:
        return midint_copy(midtype_longlong_smin());
    case MIDPAR_TYPESPEC_ULONGLONG:
        return midint_zero(midtype_longlong_size * 8);

    case MIDPAR_TYPESPEC_BOOL:
        return midint_zero(midtype_bool_size * 8);

    default:
        assert(!midsema_is_integral_typespec(spec));
        MID_CRASH("spec isn't integral");
    }
}

enum midpar_TypeSpec midsema_integral_prom(enum midpar_TypeSpec spec)
{
    assert(midsema_is_integral_typespec(spec));

    if (spec == MIDPAR_TYPESPEC_BOOL)
        return MIDPAR_TYPESPEC_INT;

    int32_t spec_rank = midsema_typespec_conv_rank(spec);
    int32_t int_rank = midsema_typespec_conv_rank(MIDPAR_TYPESPEC_INT);

    if (spec_rank < int_rank) {
        struct mid_APInt spec_max = midsema_integral_max(spec);
        struct mid_APInt spec_min = midsema_integral_min(spec);

        enum midpar_TypeSpec new_spec;

        if (midint_is_ugteq_diff_sizes(midtype_int_smax(), &spec_max) &&
            midint_is_ulteq_diff_sizes(midtype_int_smin(), &spec_min))
            new_spec = MIDPAR_TYPESPEC_INT;
        else
            new_spec = MIDPAR_TYPESPEC_UINT;

        mid_APInt_deinit(&spec_max);
        mid_APInt_deinit(&spec_min);
        return new_spec;
    } else {
        return spec;
    }
}

bool midsema_type_is_scalar(const struct midpar_Type *type)
{
    return midsema_n_indir(type) > 0 ||
           midsema_is_integral_typespec(type->spec) ||
           midsema_is_floating_typespec(type->spec);
}

bool midsema_type_is_ref(const struct midpar_Type *type)
{
    return type->lv_ref || type->rv_ref;
}

bool midsema_type_is_literal(const struct midpar_Type *type)
{
    if (midsema_type_is_scalar(type) || midsema_type_is_ref(type)) {
        return true;

    } else if (midsema_type_is_array(type)) {
        return midsema_type_is_literal(&type->array->elem);

    } else if (midsema_type_is_class_or_union(type)) {
        const struct midsema_Ident *ident =
            midsema_deref_identptr(&type->named);
        assert(ident->def);
        assert(ident->def->type == MIDPAR_ASTNODETYPE_CLASS);
        return midsema_class_is_literal(&ident->def->class_);
    } else {
        return false;
    }
}

static bool are_fptrs_same(const struct midpar_TypeFPtr *a,
                           const struct midpar_TypeFPtr *b)
{
    if (a->params.len != b->params.len)
        return false;
    else if (a->has_ellipsis != b->has_ellipsis)
        return false;
    else if (!midsema_are_types_same(&a->ret, &b->ret))
        return false;

    for (mid_isize i = 0; i < a->params.len; ++i) {
        if (!midsema_are_types_same(&a->params.arr[i], &b->params.arr[i]))
            return false;
    }

    return true;
}

static bool are_arrays_same(const struct midpar_TypeArray *a,
                            const struct midpar_TypeArray *b)
{
    if (a->len != b->len)
        return false;

    return midsema_are_types_same(&a->elem, &b->elem);
}

bool midsema_dquals_same(const struct midpar_TypeDataQual *a, mid_isize n_a,
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

bool midsema_squals_same(const struct midpar_TypeStorQual *a,
                         const struct midpar_TypeStorQual *b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

bool midsema_are_types_same(const struct midpar_Type *a,
                            const struct midpar_Type *b)
{
    if (a->spec != b->spec)
        return false;
    else if (a->lv_ref != b->lv_ref || a->rv_ref != b->rv_ref)
        return false;
    else if (!midsema_squals_same(&a->squals, &b->squals))
        return false;
    else if (!midsema_dquals_same(a->dquals.arr, a->dquals.len, b->dquals.arr,
                                  b->dquals.len))
        return false;
    else if (a->spec == MIDPAR_TYPESPEC_FPTR)
        return are_fptrs_same(a->fptr, b->fptr);
    else if (a->spec == MIDPAR_TYPESPEC_ARRAY)
        return are_arrays_same(a->array, b->array);
    else if (midsema_is_typespec_named(a->spec))
        return a->named.parent == b->named.parent &&
               a->named.idx == b->named.idx;
    else
        return true;
}

bool midsema_type_is_void(const struct midpar_Type *type)
{
    return midsema_n_indir(type) == 0 && type->spec == MIDPAR_TYPESPEC_VOID;
}

bool midsema_type_is_void_ptr(const struct midpar_Type *type)
{
    return midsema_n_indir(type) == 1 && type->spec == MIDPAR_TYPESPEC_VOID;
}

bool midsema_type_is_nullptr_t(const struct midpar_Type *type)
{
    return midsema_n_indir(type) == 0 && type->spec == MIDPAR_TYPESPEC_NULLPTR;
}

bool midsema_type_is_typecheckable(const struct midpar_Type *type)
{
    return midsema_is_typespec_typecheckable(type->spec);
}

enum midlit_ValueKind
midsema_type_lit_value_kind(const struct midpar_Type *type)
{
    if (midsema_n_indir(type) > 0)
        MID_CRASH("ptrs not supported");

    switch (type->spec) {
    case MIDPAR_TYPESPEC_CHAR:
    case MIDPAR_TYPESPEC_SCHAR:
    case MIDPAR_TYPESPEC_UCHAR:
    case MIDPAR_TYPESPEC_WCHAR:
    case MIDPAR_TYPESPEC_CHAR16:
    case MIDPAR_TYPESPEC_CHAR32:
    case MIDPAR_TYPESPEC_SHORT:
    case MIDPAR_TYPESPEC_INT:
    case MIDPAR_TYPESPEC_LONG:
    case MIDPAR_TYPESPEC_LONGLONG:
    case MIDPAR_TYPESPEC_BOOL:
        return MIDLIT_VALUE_SIGNED_INT;

    case MIDPAR_TYPESPEC_USHORT:
    case MIDPAR_TYPESPEC_UINT:
    case MIDPAR_TYPESPEC_ULONG:
    case MIDPAR_TYPESPEC_ULONGLONG:
        return MIDLIT_VALUE_UNSIGNED_INT;

    case MIDPAR_TYPESPEC_FLOAT:
    case MIDPAR_TYPESPEC_DOUBLE:
    case MIDPAR_TYPESPEC_LONGDOUBLE:
        return MIDLIT_VALUE_FLOAT;

    default:
        MID_CRASH("not a literal value");
    }
}

enum midflt_Kind midsema_get_flt_kind(enum midpar_TypeSpec spec)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_FLOAT:
        return midtype_float_kind;

    case MIDPAR_TYPESPEC_DOUBLE:
        return midtype_double_kind;

    case MIDPAR_TYPESPEC_LONGDOUBLE:
        return midtype_longdouble_kind;

    default:
        MID_CRASH("type spec is not floating point");
    }
}

int_least32_t midsema_typespec_size(enum midpar_TypeSpec spec)
{
    switch (spec) {
    case MIDPAR_TYPESPEC_CHAR:
    case MIDPAR_TYPESPEC_SCHAR:
    case MIDPAR_TYPESPEC_UCHAR:
        return midtype_char_size;

    case MIDPAR_TYPESPEC_WCHAR:
        return midtype_wchar_size;

    case MIDPAR_TYPESPEC_CHAR16:
        return 2;

    case MIDPAR_TYPESPEC_CHAR32:
        return 4;

    case MIDPAR_TYPESPEC_SHORT:
    case MIDPAR_TYPESPEC_USHORT:
        return midtype_short_size;

    case MIDPAR_TYPESPEC_INT:
    case MIDPAR_TYPESPEC_UINT:
        return midtype_int_size;

    case MIDPAR_TYPESPEC_LONG:
    case MIDPAR_TYPESPEC_ULONG:
        return midtype_long_size;

    case MIDPAR_TYPESPEC_LONGLONG:
    case MIDPAR_TYPESPEC_ULONGLONG:
        return midtype_longlong_size;

    case MIDPAR_TYPESPEC_FLOAT:
        return midtype_float_size;

    case MIDPAR_TYPESPEC_DOUBLE:
        return midtype_double_size;

    case MIDPAR_TYPESPEC_LONGDOUBLE:
        return midtype_longdouble_size;

    default:
        MID_CRASH("can't get size of type spec");
    }
}

struct mid_APInt midsema_type_size(const struct midpar_Type *type)
{
    if (midsema_n_indir(type) || type->spec == MIDPAR_TYPESPEC_NULLPTR)
        return midint_init(64, midtype_ptr_size, false);

    switch (type->spec) {
    case MIDPAR_TYPESPEC_CHAR:
    case MIDPAR_TYPESPEC_SCHAR:
    case MIDPAR_TYPESPEC_UCHAR:
        return midint_init(64, midtype_char_size, false);

    case MIDPAR_TYPESPEC_WCHAR:
        return midint_init(64, midtype_wchar_size, false);

    case MIDPAR_TYPESPEC_CHAR16:
        return midint_init(64, 2, false);

    case MIDPAR_TYPESPEC_CHAR32:
        return midint_init(64, 4, false);

    case MIDPAR_TYPESPEC_SHORT:
    case MIDPAR_TYPESPEC_USHORT:
        return midint_init(64, midtype_short_size, false);

    case MIDPAR_TYPESPEC_INT:
    case MIDPAR_TYPESPEC_UINT:
        return midint_init(64, midtype_int_size, false);

    case MIDPAR_TYPESPEC_LONG:
    case MIDPAR_TYPESPEC_ULONG:
        return midint_init(64, midtype_long_size, false);

    case MIDPAR_TYPESPEC_LONGLONG:
    case MIDPAR_TYPESPEC_ULONGLONG:
        return midint_init(64, midtype_longlong_size, false);

    case MIDPAR_TYPESPEC_FLOAT:
        return midint_init(64, midtype_float_size, false);

    case MIDPAR_TYPESPEC_DOUBLE:
        return midint_init(64, midtype_double_size, false);

    case MIDPAR_TYPESPEC_LONGDOUBLE:
        return midint_init(64, midtype_longdouble_size, false);

    case MIDPAR_TYPESPEC_ARRAY: {
        struct mid_APInt elem_size = midsema_type_size(&type->array->elem);
        midint_mul_uimm(&elem_size, type->array->len);
        return elem_size;
    }

    default:
        MID_CRASH("type doesn't have a size");
    }
}

struct mid_APInt midsema_sizeof_type(const struct midpar_Type *type)
{
    struct mid_APInt bytes = midsema_type_size(type);
    auto char_bytes = midint_init(64, midtype_char_size, false);
    midint_udiv(&bytes, &char_bytes);

    mid_APInt_deinit(&char_bytes);
    return bytes;
}

bool midsema_type_is_class_or_union(const struct midpar_Type *type)
{
    return midsema_n_indir(type) == 0 && (type->spec == MIDPAR_TYPESPEC_CLASS ||
                                          type->spec == MIDPAR_TYPESPEC_UNION);
}

bool midsema_type_is_array(const struct midpar_Type *type)
{
    return midsema_n_indir(type) == 0 && type->spec == MIDPAR_TYPESPEC_ARRAY;
}

static bool class_type_has_trivial_dtor(const struct midpar_Type *type)
{
    const struct midsema_Ident *ident = midsema_deref_identptr(&type->named);
    assert(ident->def);
    assert(ident->def->type == MIDPAR_ASTNODETYPE_CLASS);

    return midsema_has_trivial_dtor(&ident->def->class_);
}

bool midsema_type_has_trivial_dtor(const struct midpar_Type *type)
{
    if (midsema_type_is_class_or_union(type))
        return class_type_has_trivial_dtor(type);
    else if (midsema_type_is_array(type))
        return class_type_has_trivial_dtor(&type->array->elem);
    else
        return true;
}

static bool class_type_trivially_constructible(const struct midpar_Type *type)
{
    const struct midsema_Ident *ident = midsema_deref_identptr(&type->named);
    assert(ident->def);
    assert(ident->def->type == MIDPAR_ASTNODETYPE_CLASS);

    return midsema_class_is_trivially_constructible(&ident->def->class_);
}

bool midsema_type_is_trivially_constructible(const struct midpar_Type *type)
{
    if (midsema_type_is_class_or_union(type))
        return class_type_trivially_constructible(type);
    else if (midsema_type_is_array(type) &&
             midsema_type_is_class_or_union(&type->array->elem))
        return class_type_trivially_constructible(&type->array->elem);
    else
        return true;
}

static bool class_type_has_trivial_default_ctor(const struct midpar_Type *type)
{
    const struct midsema_Ident *ident = midsema_deref_identptr(&type->named);
    assert(ident->def);
    assert(ident->def->type == MIDPAR_ASTNODETYPE_CLASS);

    return midsema_has_trivial_default_ctor(&ident->def->class_);
}

bool midsema_type_has_trivial_default_ctor(const struct midpar_Type *type)
{
    if (midsema_type_is_class_or_union(type))
        return class_type_has_trivial_default_ctor(type);
    else if (midsema_type_is_array(type) &&
             midsema_type_is_class_or_union(&type->array->elem))
        return class_type_has_trivial_default_ctor(&type->array->elem);
    else if (midsema_type_is_ref(type))
        return false;
    else
        return true;
}
