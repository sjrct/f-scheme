#ifndef VALUE_H
#define VALUE_H

struct Value;
typedef struct Value Value;

#include <stdio.h>
#include "env.h"
#include "number.h"

enum Type {
    TYPE_NULL = 0, // This is actually represented by a NULL ptr, not ->type == TYPE_NULL
    TYPE_ATOM,
    TYPE_NUMBER,
    TYPE_LIST,
    TYPE_FUNCTION,
    TYPE_FUNCTION_SF,
    TYPE_BUILTIN,
    TYPE_BUILTIN_SF,
    TYPE_BOOLEAN,
    TYPE_EXCEPTION,
    TYPE_BOUND_EXCEPTION,
    TYPE_STRING,
};

extern const char *type_names[];

struct List {
    Value *car;
    Value *cdr;
};

struct Function {
    Value *operands, *body;
    Env *env;
};

typedef Value *(*Builtin)(Value *arg, Env *env);

struct Value {
    enum Type type;
    union {
        char *atom;
        Number number;
        struct List list;
        struct Function func;
        Builtin builtin;
        int boolean;
        char *exception;
        char *string;
    } value;

    // Garbage collection
    int refs;
    Value *gc_next;
    Value *gc_prev;
};

extern Value vtrue, vfalse;
extern Value *gc_head;

#define TYPEOF(V) ((V) == NULL ? TYPE_NULL : (V)->type)
#define IS_LIST(V) ((V) == NULL || (V)->type == TYPE_LIST)
#define IS_FUNCTION(V) ((V) != NULL && ((V)->type == TYPE_FUNCTION || (V)->type == TYPE_FUNCTION_SF))
#define IS_BUILTIN(V) ((V) != NULL && ((V)->type == TYPE_BUILTIN || (V)->type == TYPE_BUILTIN_SF))
#define IS_CALLABLE(V) (IS_FUNCTION(V) || IS_BUILTIN(V))
#define IS_EXCEPTION(V) ((V) != NULL && ((V)->type == TYPE_EXCEPTION || (V)->type == TYPE_BOUND_EXCEPTION))

#define TRUE  (&vtrue)
#define FALSE (&vfalse)

Value *cons(Value *, Value *);
Value *create_value(enum Type type);
Value *create_number(Number);
Value *create_atom(const char *str);
Value *create_atom_alloced(char *str);
Value *create_atom_list(const char **list, int len);
Value *create_builtin(Builtin);
Value *create_builtin_sf(Builtin);
Value *create_string(char *str);
Value *create_string_alloced(char *str);
Value *create_exception(const char *s, ...);
Value *copy_value(Value *v);
int delete_value(Value *v);
int values_equal(Value *, Value *);

void track_value(Value *v);
void untrack_value(Value *v);
int is_tracked(Value *v);

#define CAR(V) ((V)->value.list.car)
#define CDR(V) ((V)->value.list.cdr)

static inline Value *car(Value * v) {
    if (v == NULL || v->type != TYPE_LIST) return v;
    return CAR(v);
}

static inline Value *cdr(Value * v) {
    if (v == NULL || v->type != TYPE_LIST) return NULL;
    return CDR(v);
}

static inline void _debug(const char *name, Value *v) {
    void print(Value *);
    printf("%s = ", name);
    print(v);
    puts("");
}

#define debug(V) (_debug(#V, V))

#endif
