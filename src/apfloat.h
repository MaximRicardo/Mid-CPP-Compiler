#pragma once

#include "apint.h"
#include "ints.h"

enum MidAPFloat_IEEEKind {
    MIDAPFLOAT_IEEE_HALF,
    MIDAPFLOAT_IEEE_SINGLE,
    MIDAPFLOAT_IEEE_DOUBLE,
};

struct MidAPFloat_IEEE {
    struct MidAPInt mant; // mantissa
    i64 exp;              // exponent
    enum MidAPFloat_IEEEKind kind;
    bool is_neg; // sign bit
};

void MidAPFloat_IEEE_deinit(struct MidAPFloat_IEEE *self);
bool MidAPFloat_IEEE_is_zero(const struct MidAPFloat_IEEE *self);
bool MidAPFloat_IEEE_is_inf(const struct MidAPFloat_IEEE *self);
bool MidAPFloat_IEEE_is_nan(const struct MidAPFloat_IEEE *self);

enum MidAPFloat_Kind {
    MIDAPFLOAT_IEEE,
};

struct MidAPFloat {
    union {
        struct MidAPFloat_IEEE ieee;
    };
    enum MidAPFloat_Kind kind;
};

void MidAPFloat_deinit(struct MidAPFloat *self);
