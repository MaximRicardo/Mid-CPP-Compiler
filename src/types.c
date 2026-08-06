#include "types.h"
#include "apint.h"
#include <assert.h>

static struct mid_APInt char_smax, char_smin, char_umax;
static struct mid_APInt wchar_smax, wchar_smin, wchar_umax;
static struct mid_APInt short_smax, short_smin, short_umax;
static struct mid_APInt int_smax, int_smin, int_umax;
static struct mid_APInt long_smax, long_smin, long_umax;
static struct mid_APInt longlong_smax, longlong_smin, longlong_umax;
static struct mid_APInt ptr_smax, ptr_smin, ptr_umax;

static bool module_inited = false;

void midtype_init_module()
{
    module_inited = true;

    char_smax = midint_init(midtype_char_size * 8, INT8_MAX, true);
    char_smin = midint_init(midtype_char_size * 8, INT8_MIN, true);
    char_umax = midint_init(midtype_char_size * 8, UINT8_MAX, false);

    wchar_smax = midint_init(midtype_wchar_size * 8, INT32_MAX, true);
    wchar_smin = midint_init(midtype_wchar_size * 8, INT32_MIN, true);
    wchar_umax = midint_init(midtype_wchar_size * 8, UINT32_MAX, false);

    short_smax = midint_init(midtype_short_size * 8, INT16_MAX, true);
    short_smin = midint_init(midtype_short_size * 8, INT16_MIN, true);
    short_umax = midint_init(midtype_short_size * 8, UINT16_MAX, false);

    int_smax = midint_init(midtype_int_size * 8, INT32_MAX, true);
    int_smin = midint_init(midtype_int_size * 8, INT32_MIN, true);
    int_umax = midint_init(midtype_int_size * 8, UINT32_MAX, false);

    long_smax = midint_init(midtype_long_size * 8, INT64_MAX, true);
    long_smin = midint_init(midtype_long_size * 8, INT64_MIN, true);
    long_umax = midint_init(midtype_long_size * 8, UINT64_MAX, false);

    longlong_smax = midint_init(midtype_longlong_size * 8, INT64_MAX, true);
    longlong_smin = midint_init(midtype_longlong_size * 8, INT64_MIN, true);
    longlong_umax = midint_init(midtype_longlong_size * 8, UINT64_MAX, false);

    ptr_smax = midint_init(midtype_ptr_size * 8, INT64_MAX, true);
    ptr_smin = midint_init(midtype_ptr_size * 8, INT64_MIN, true);
    ptr_umax = midint_init(midtype_ptr_size * 8, UINT64_MAX, false);
}

const struct mid_APInt *midtype_char_smax()
{
    assert(module_inited);
    return &char_smax;
}

const struct mid_APInt *midtype_char_umax()
{
    assert(module_inited);
    return &char_umax;
}

const struct mid_APInt *midtype_char_smin()
{
    assert(module_inited);
    return &char_smin;
}

const struct mid_APInt *midtype_wchar_smax()
{
    assert(module_inited);
    return &wchar_smax;
}

const struct mid_APInt *midtype_wchar_umax()
{
    assert(module_inited);
    return &wchar_umax;
}

const struct mid_APInt *midtype_wchar_smin()
{
    assert(module_inited);
    return &wchar_smin;
}

const struct mid_APInt *midtype_short_smax()
{
    assert(module_inited);
    return &short_smax;
}

const struct mid_APInt *midtype_short_umax()
{
    assert(module_inited);
    return &short_umax;
}

const struct mid_APInt *midtype_short_smin()
{
    assert(module_inited);
    return &short_smin;
}

const struct mid_APInt *midtype_int_smax()
{
    assert(module_inited);
    return &int_smax;
}

const struct mid_APInt *midtype_int_umax()
{
    assert(module_inited);
    return &int_umax;
}

const struct mid_APInt *midtype_int_smin()
{
    assert(module_inited);
    return &int_smin;
}

const struct mid_APInt *midtype_long_smax()
{
    assert(module_inited);
    return &long_smax;
}

const struct mid_APInt *midtype_long_umax()
{
    assert(module_inited);
    return &long_umax;
}

const struct mid_APInt *midtype_long_smin()
{
    assert(module_inited);
    return &long_smin;
}

const struct mid_APInt *midtype_longlong_smax()
{
    assert(module_inited);
    return &longlong_smax;
}

const struct mid_APInt *midtype_longlong_umax()
{
    assert(module_inited);
    return &longlong_umax;
}

const struct mid_APInt *midtype_longlong_smin()
{
    assert(module_inited);
    return &longlong_smin;
}

const struct mid_APInt *midtype_ptr_smax()
{
    assert(module_inited);
    return &ptr_smax;
}

const struct mid_APInt *midtype_ptr_umax()
{
    assert(module_inited);
    return &ptr_umax;
}

const struct mid_APInt *midtype_ptr_smin()
{
    assert(module_inited);
    return &ptr_smin;
}
