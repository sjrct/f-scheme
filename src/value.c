#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "value.h"

const char *type_names[] = {
    "null",
    "atom",
    "number",
    "list",
    "function",
    "function*",
    "builtin",
    "builtin*",
    "boolean",
    "exception*",
    "exception",
    "string",
};

Value vtrue = {
    .type = TYPE_BOOLEAN,
    .value.boolean = 1,
    .refs = 1,
    .gc_next = NULL,
    .gc_prev = NULL,
};

Value vfalse = {
    .type = TYPE_BOOLEAN,
    .value.boolean = 0,
    .refs = 1,
    .gc_next = NULL,
    .gc_prev = NULL,
};

Value *gc_head = NULL;

Value *create_value(enum Type type) {
    Value *v = calloc(1, sizeof *v);
    v->type = type;
    v->refs = 1;
    track_value(v);
    return v;
}

void track_value(Value *v) {
    if (gc_head) gc_head->gc_prev = v;
    v->gc_next = gc_head;
    v->gc_prev = NULL;
    gc_head = v;
}

int is_tracked(Value *v) {
    return v->gc_prev || v->gc_next || v == gc_head;
}

void untrack_value(Value *v) {
    if (v != NULL) {
        if (v == gc_head) gc_head = v->gc_next;
        if (v->gc_prev) {
            v->gc_prev->gc_next = v->gc_next;
            v->gc_prev = NULL;
        }
        if (v->gc_next) {
            v->gc_next->gc_prev = v->gc_prev;
            v->gc_next = NULL;
        }
    }
}

Value *create_number(Number n) {
    Value *v = create_value(TYPE_NUMBER);
    v->value.number = n;
    return v;
}

Value *create_atom(const char *str) {
    return create_atom_alloced(strdup(str));
}

Value *create_atom_alloced(char *str) {
    Value *v = create_value(TYPE_ATOM);
    v->value.atom = str;
    return v;
}

Value *create_string(char *str) {
    return create_string_alloced(strdup(str));
}

Value *create_string_alloced(char *str) {
    Value *v = create_value(TYPE_STRING);
    v->value.string = str;
    return v;
}

// Doesn't incr ref counter
Value *cons(Value *h, Value *t) {
    Value *ls = create_value(TYPE_LIST);
    CAR(ls) = h;

    // Enforce cdrs being always lists
    if (IS_LIST(t)) {
        CDR(ls) = t;
    } else {
        CDR(ls) = cons(t, NULL);
    }

    return ls;
}


Value *create_atom_list(const char **list, int len) {
    Value *ls = NULL;
    Value **next = &ls;

    for (int i = 0; i < len; i++) {
        *next = cons(create_atom(list[i]), NULL);
        next = &CDR(*next);
    }

    return ls;
}

Value *create_builtin(Builtin func) {
    Value *v = create_value(TYPE_BUILTIN);
    v->value.builtin = func;
    return v;
}

Value *create_builtin_sf(Builtin func) {
    Value *v = create_builtin(func);
    v->type = TYPE_BUILTIN_SF;
    return v;
}

Value *create_exception(const char *tmpl, ...) {
    va_list args;
    size_t size;
    char *text;
    Value *v;

    va_start(args, tmpl);
    size = vsnprintf(NULL, 0, tmpl, args) + 1;
    va_end(args);

    text = malloc(size);
    va_start(args, tmpl);
    vsnprintf(text, size, tmpl, args);
    va_end(args);

    v = create_value(TYPE_EXCEPTION);
    v->value.exception = text;
    return v;
}

Value *copy_value(Value *v) {
    if (v != NULL) v->refs += 1;
    return v;
}

int delete_value(Value *v) {
    if (v == NULL) return 0;

    if (!--v->refs) {
        untrack_value(v);

        if (v->type == TYPE_LIST) {
            delete_value(v->value.list.car);
            delete_value(v->value.list.cdr);
        } else if (v->type == TYPE_ATOM) {
            free(v->value.atom);
        } else if (v->type == TYPE_EXCEPTION) {
            free(v->value.exception);
        }
        free(v);
        return 0;
    }

    return v->refs;
}

int values_equal(Value *a, Value *b) {

    if (a->type != b->type) return 0;

    switch (a->type) {
    case TYPE_ATOM:
        return !strcmp(a->value.atom, b->value.atom);
    case TYPE_STRING:
        return !strcmp(a->value.string, b->value.string);
    case TYPE_NUMBER:
        return eq_number(a->value.number, b->value.number);
    case TYPE_BOOLEAN:
        return a->value.boolean == b->value.boolean;
    case TYPE_LIST:
        return values_equal(car(a), car(b)) && values_equal(car(a), car(b));
    case TYPE_FUNCTION:
    case TYPE_FUNCTION_SF:
        // FIXME different parameters?
        return values_equal(a->value.func.operands, b->value.func.operands)
            && values_equal(a->value.func.body, b->value.func.body)
            && a->value.func.env == b->value.func.env;
    case TYPE_BUILTIN:
    case TYPE_BUILTIN_SF:
        return a->value.builtin == b->value.builtin;
    case TYPE_EXCEPTION:
    case TYPE_BOUND_EXCEPTION:
        return !strcmp(a->value.exception, b->value.exception);
    case TYPE_NULL:
        return 1;
    }

    assert(0);
}
