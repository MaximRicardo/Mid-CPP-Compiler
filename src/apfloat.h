#pragma once

#include "apint.h"
#include "ints.h"

enum midflt_IEEEKind {
    MIDFLT_IEEE_HALF,
    MIDFLT_IEEE_SINGLE,
    MIDFLT_IEEE_DOUBLE,
};

struct midflt_IEEE {
    struct mid_APInt mant; // mantissa
    i64 exp;               // exponent
    enum midflt_IEEEKind kind;
    bool is_neg; // sign bit
};

void midflt_IEEE_deinit(struct midflt_IEEE *self);
bool midflt_IEEE_is_zero(const struct midflt_IEEE *self);
bool midflt_IEEE_is_inf(const struct midflt_IEEE *self);
bool midflt_IEEE_is_nan(const struct midflt_IEEE *self);

enum midflt_Kind {
    MIDFLT_IEEE,
};

struct mid_APFloat {
    union {
        struct midflt_IEEE ieee;
    };
    enum midflt_Kind kind;
};

void midflt_deinit(struct mid_APFloat *self);
