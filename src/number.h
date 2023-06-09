#ifndef NUMBER_H
#define NUMBER_H

struct Number;
typedef struct Number Number;

enum Number_Type {
    NUMBER_LLONG,
    NUMBER_DOUBLE,
};

struct Number {
    enum Number_Type type;
    union {
        long long ll;
        double d;
    } v;
};

Number create_number_ll(long long);
Number create_number_d(double);
Number parse_number(const char **);

Number add_number(Number, Number);
Number sub_number(Number, Number);
Number mul_number(Number, Number);
Number div_number(Number, Number);
Number rem_number(Number, Number);

int eq_number(Number, Number);
int lt_number(Number, Number);
int lte_number(Number, Number);
#define gt_number(A, B) lt_number(B, A)
#define gte_number(A, B) lte_number(B, A)

Number floor_number(Number);
Number ceil_number(Number);


#endif
