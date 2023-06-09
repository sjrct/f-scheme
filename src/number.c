#include <ctype.h>
#include <math.h>
#include "number.h"

Number create_number_ll(long long x) {
    Number n = {
        .type = NUMBER_LLONG,
        .v.ll = x
    };
    return n;
}

Number create_number_d(double x) {
    Number n = {
        .type = NUMBER_DOUBLE,
        .v.d = x
    };
    return n;
}

// TODO scientific notation
Number parse_number(const char **pstr) {
    double after_decimal = 0.;
    long long neg = 1;
    Number n = create_number_ll(0);

    if (**pstr == '-') {
        neg = -1;
        ++*pstr;
    }

    while (isdigit(**pstr) || **pstr == '.') {
        if (**pstr == '.') {
            after_decimal = .1;
            if (n.type == NUMBER_LLONG) {
                n.type = NUMBER_DOUBLE;
                n.v.d = n.v.ll;
            }
        } else if (after_decimal != 0.) {
            n = add_number(n,
                    mul_number(
                        create_number_ll(**pstr - '0'),
                        create_number_d(after_decimal)));
            after_decimal *= .1;
        } else {
            n = mul_number(n, create_number_ll(10));
            n = add_number(n, create_number_ll(**pstr - '0'));
        }
        ++*pstr;
    }

    return mul_number(n, create_number_ll(neg));
}

static void cast_types(Number a_in, Number b_in, Number *a_out, Number *b_out) {
    if (a_in.type == b_in.type) {
        *a_out = a_in;
        *b_out = b_in;
    } else if (a_in.type == NUMBER_DOUBLE) {
        *a_out = a_in;
        *b_out = create_number_d(b_in.v.ll);
    } else {
        *a_out = create_number_d(a_in.v.ll);
        *b_out = b_in;
    }
}

#define ARITH(FUNC_NAME, INT_OP, DBL_OP) \
Number FUNC_NAME(Number a0, Number b0) { \
    long long res; \
    Number a, b; \
    cast_types(a0, b0, &a, &b); \
    \
    if (a.type == NUMBER_LLONG) { \
        if (INT_OP(a.v.ll, b.v.ll, &res)) { \
            /* overflow, use doubles */ \
            a = create_number_d(a.v.ll); \
            b = create_number_d(b.v.ll); \
        } else { \
            return create_number_ll(res); \
        } \
    } \
    \
    return create_number_d(a.v.d DBL_OP b.v.d); \
}

ARITH(add_number, __builtin_saddll_overflow, +)
ARITH(sub_number, __builtin_ssubll_overflow, -)
ARITH(mul_number, __builtin_smulll_overflow, *)

Number div_number(Number a0, Number b0) {
    Number a, b;
    cast_types(a0, b0, &a, &b);

    if (a.type == NUMBER_LLONG) {
        return create_number_ll(a.v.ll / b.v.ll);
    } else {
        return create_number_d(a.v.d / b.v.d);
    }
}

Number rem_number(Number a0, Number b0) {
    Number a, b;
    cast_types(a0, b0, &a, &b);

    if (a.type == NUMBER_LLONG) {
        return create_number_ll(a.v.ll % b.v.ll);
    } else {
        double res = a.v.d / b.v.d;
        double decimal = res - floor(res);
        return create_number_d(b.v.d * decimal);
    }
}

#define COMP(NAME, OP) \
int NAME(Number a0, Number b0) { \
    Number a, b; \
    cast_types(a0, b0, &a, &b); \
    \
    if (a.type == NUMBER_LLONG) { \
        return a.v.ll OP b.v.ll; \
    } else { \
        return a.v.d OP b.v.d; \
    } \
}

COMP(eq_number, ==)
COMP(lt_number, <)
COMP(lte_number, <=)

Number floor_number(Number a) {
    if (a.type == NUMBER_LLONG) {
        return a;
    } else {
        return create_number_ll(floor(a.v.d));
    }
}

Number ceil_number(Number a) {
    if (a.type == NUMBER_LLONG) {
        return a;
    } else {
        return create_number_ll(ceil(a.v.d));
    }
}
