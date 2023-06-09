#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "builtins.h"
#include "env.h"
#include "interpreter.h"

#define ARITH_POS(OPER, INIT) \
static Value *bltn_ ## OPER(Value *args, Env *env) { \
    Number total = create_number_ll(INIT); \
    \
    while (args != NULL) { \
        assert(args->type == TYPE_LIST); \
        if (TYPEOF(car(args)) != TYPE_NUMBER) { \
           return create_exception("Can only perform arithmetic on numbers"); \
        } \
        total = OPER ## _number(total, car(args)->value.number); \
        args = cdr(args); \
    } \
    \
    return create_number(total); \
}

ARITH_POS(add, 0)
ARITH_POS(mul, 1)

#define ARITH_NEG(OPER, INIT) \
static Value *bltn_ ## OPER(Value *args, Env *env) { \
    Number total; \
    \
    if (TYPEOF(car(args)) != TYPE_NUMBER) { \
       return create_exception("First argument to - or / must be number"); \
    } \
    \
    total = car(args)->value.number; \
    args = cdr(args); \
    \
    /* special case unary */ \
    if (args == NULL) { \
        return create_number(OPER ## _number(create_number_ll(INIT), total)); \
    } \
    \
    while (args != NULL) { \
        assert(args->type == TYPE_LIST); \
        if (TYPEOF(car(args)) != TYPE_NUMBER) { \
           return create_exception("Can only perform arithmetic on numbers"); \
        } \
        total = OPER ## _number(total, car(args)->value.number); \
        args = cdr(args); \
    } \
    \
    return create_number(total); \
}

ARITH_NEG(sub, 0)
ARITH_NEG(div, 1)
ARITH_NEG(rem, 0)

// FIXME
static Value *quote(Value *args, Env *env) {
    return copy_value(car(args));
}

Value *validate_lambda_arguments(Value *operands, const char *expr) {
    // Validate arguments
    if (!IS_LIST(operands)) {
       debug(operands);
        return create_exception("Operands list in %s must be list", expr);
    } else {
        for (Value *op = operands; op != NULL; op = cdr(op)) {
            if (TYPEOF(car(op)) != TYPE_ATOM) {
                debug(operands);
                return create_exception("Operands list in %s must be list of atoms", expr);
            }
        }
    }
    return NULL;
}

Value *make_func(Value *operands, Value *body, enum Type type, Env *env, const char *func_name) {
    Value *err, *func;

    err = validate_lambda_arguments(operands, func_name);
    if (err) return err;

    func = create_value(type);
    func->value.func.operands = copy_value(operands);
    func->value.func.body = copy_value(body);
    func->value.func.env = copy_env(env);
    return func;
}

#define SET_OR_DEFINE(WHICH, FUNC) \
static Value *WHICH(Value *args, Env *env) { \
    Value *name = car(args); \
    Value *value; \
    \
    if (TYPEOF(name) == TYPE_LIST) { \
        Value *operands = cdr(name); \
        Value *body = car(cdr(args)); \
        name = car(name); \
        \
        value = make_func(operands, body, TYPE_FUNCTION, env, #WHICH); \
        if (TYPEOF(value) == TYPE_EXCEPTION) return value; \
    } else { \
        value = eval(car(cdr(args)), env); \
    }\
    \
    /* Validate arguments */ \
    if (TYPEOF(name) != TYPE_ATOM) { \
        delete_value(value); \
        return create_exception("Name in " #WHICH " expression can only be an atom"); \
    } \
    \
    FUNC(env, name->value.atom, value); \
    \
    return NULL; \
}

SET_OR_DEFINE(define, add_to_env)
SET_OR_DEFINE(set, set_in_env)

static Value *lambda(Value *args, Env *env) {
    if (cdr(args) == NULL) {
        return create_exception("lambda must have a body");
    }

    Value *operands = car(args);
    Value *body = car(cdr(args));

    return make_func(operands, body, TYPE_FUNCTION, env, "lambda");
}

static Value *macro(Value *args, Env *env) {
    if (cdr(args) == NULL) {
        return create_exception("macro must have a body");
    }

    Value *operands = car(args);
    Value *body = car(cdr(args));

    return make_func(operands, body, TYPE_FUNCTION_SF, env, "macro");
}

static Value *equal(Value *args, Env *env) {
    if (args == NULL) {
        return copy_value(&vtrue);
    }

    Value *first = car(args);
    args = cdr(args);

    while (args != NULL) {
        if (!values_equal(car(args), first)) {
            return copy_value(&vfalse);
        }
        args = cdr(args);
    }

    return copy_value(&vtrue);
}

static Value *cond(Value *args, Env *env) {
    while (args != NULL) {
        Value *clause = car(args);

        if (TYPEOF(clause) != TYPE_LIST || cdr(clause) == NULL) {
            return create_exception("cond arguments must be 2-element lists");
        }

        Value *b = eval(car(clause), env);
        if (TYPEOF(b) == TYPE_BOOLEAN && b->value.boolean) {
            Value *ret = eval(car(cdr(clause)), env);
            delete_value(b);
            return ret;
        }

        delete_value(b);
        args = cdr(args);
    }

    return NULL;
}

#define SIMPLE_PRED(NAME, CHECK) \
Value *NAME(Value *args, Env *e) { \
    if (CHECK) { \
        return copy_value(TRUE); \
    } else { \
        return copy_value(FALSE); \
    } \
}

SIMPLE_PRED(is_null, TYPEOF(car(args)) == TYPE_NULL);
SIMPLE_PRED(is_atom, TYPEOF(car(args)) == TYPE_ATOM);
SIMPLE_PRED(is_list, TYPEOF(car(args)) == TYPE_LIST);
SIMPLE_PRED(is_number, TYPEOF(car(args)) == TYPE_NUMBER);
SIMPLE_PRED(is_boolean, TYPEOF(car(args)) == TYPE_BOOLEAN);
SIMPLE_PRED(is_exception, IS_EXCEPTION(car(args)));
SIMPLE_PRED(is_function, IS_CALLABLE(car(args)))
SIMPLE_PRED(is_string, TYPEOF(car(args)) == TYPE_STRING);

Value *bltn_car(Value *args, Env *env) {
    return copy_value(car(car(args)));
}

Value *bltn_cdr(Value *args, Env *env) {
    return copy_value(cdr(car(args)));
}

Value *bltn_cons(Value *args, Env *env) {
    return cons(
        copy_value(car(args)),
        copy_value(car(cdr(args)))
    );
}

Value *bltn_print(Value *args, Env *env) {
   void print(Value *v);

   while (args != NULL) {
       print(car(args));
       args = cdr(args);
       if (args != NULL) printf(" ");
    }

   puts("");
   return NULL;
}

Value *trycatch(Value *args, Env *env) {
   Value *apply_func(Value *func, Value *args, Env *env);
   Value *res = eval(car(args), env);

   // Only check for free exceptions
   if (TYPEOF(res) == TYPE_EXCEPTION) {
      Value *catch = eval(car(cdr(args)), env);

      if (!IS_CALLABLE(catch)) {
         delete_value(res);
         return create_exception("second argument to try must be function, not ", type_names[TYPEOF(catch)]);
      }

      // Bind the exception so it won't bubble
      res->type = TYPE_BOUND_EXCEPTION;

      Value *catch_res = apply_func(catch, res, env);
      delete_value(res);
      return catch_res;
   } else {
      return res;
   }
}

#define COMP(OPER) \
Value *OPER(Value *args, Env *env) { \
   Value *prev = NULL; \
   while (args != NULL) { \
      Value *curr = car(args); \
      \
      if (TYPEOF(curr) != TYPE_NUMBER) { \
         return create_exception("Comparisons only works with numbers"); \
      } \
      \
      if (prev != NULL && !(OPER ## _number(prev->value.number, curr->value.number))) { \
         return copy_value(FALSE); \
      } \
      \
      prev = curr; \
      args = cdr(args); \
   } \
   return copy_value(TRUE); \
}

COMP(lt)
COMP(gt)
COMP(lte)
COMP(gte)

Value *print_env(Value *args, Env *env) {
   EnvElem *elm;
   int frame = 0;

   while (env != NULL) {
      elm = env->first;
      printf("frame %d:\n", frame);

      while (elm != NULL) {
         printf("  %s = ", elm->name);
         print(elm->value);
         puts("");
         elm = elm->next;
      }

      env = env->parent;
      frame += 1;
   }

   return NULL;
}

Value *bltn_random(Value *args, Env *env) {
    Value *max = car(args);

    if (TYPEOF(max) != TYPE_NUMBER) {
        return create_exception("random expects number");
    }
    long long nmax = floor_number(max->value.number).v.ll;
    return create_number(create_number_ll(rand() % nmax));
}

Value *bltn_include(Value *args, Env *env) {
    Value *ret = NULL;
    Value *run_script(const char *filename, Env *env);

    while (args != NULL) {
        delete_value(ret);

        if (TYPEOF(car(args)) != TYPE_STRING) {
            return create_exception("include only accepts strings");
        }

        ret = run_script(car(args)->value.string, env);
        args = cdr(args);
    }

    return ret;
}

Value *bltn_raise(Value *args, Env *env) {
    Value *v = car(args);
    if (TYPEOF(v) == TYPE_STRING) {
        return create_exception(v->value.string);
    } else if (IS_EXCEPTION(v)) {
        // Unbind exception
        // TODO copy?
        v->type = TYPE_EXCEPTION;
        return copy_value(v);
    } else {
        return create_exception("raise expects a string or exception as an argument");
    }
}

Value *string_to_number(Value *args, Env *env) {
    const char *str;
    Value *ls = NULL;
    Value **next = &ls;

    while (args != NULL) {
        if (TYPEOF(car(args)) != TYPE_STRING) {
            return create_exception("string->number expects a string as an argument");
        }
        str = car(args)->value.string;

        // FIXME parse errors
        *next = cons(create_number(parse_number(&str)), NULL);
        next = &CDR(*next);
        args = cdr(args);
    }

    if (cdr(ls) == NULL) {
        Value *ret = copy_value(car(ls));
        delete_value(ls);
        return ret;
    }
    return ls;
}

Value *number_to_string(Value *args, Env *env) {
    char *str;
    size_t len;
    Value *ls = NULL;
    Value **next = &ls;

    while (args != NULL) {
        if (TYPEOF(car(args)) != TYPE_NUMBER) {
            return create_exception("number->string expects a number as an argument");
        }

        if (car(args)->value.number.type == NUMBER_LLONG) {
            len = snprintf(NULL, 0, "%lld\n", car(args)->value.number.v.ll);
            str = malloc(len + 1);
            sprintf(str, "%lld", car(args)->value.number.v.ll);
        } else {
            len = snprintf(NULL, 0, "%g\n", car(args)->value.number.v.d);
            str = malloc(len + 1);
            sprintf(str, "%g", car(args)->value.number.v.d);
        }

        // FIXME parse errors
        *next = cons(create_string_alloced(str), NULL);
        next = &CDR(*next);
        args = cdr(args);
    }

    if (cdr(ls) == NULL) {
        Value *ret = copy_value(car(ls));
        delete_value(ls);
        return ret;
    }
    return ls;
}

Value *concat(Value *args, Env *env) {
    size_t len = 0, sz = 16, src_len;
    char *s = malloc(sz + 1);

    while (args != NULL) {
        if (TYPEOF(car(args)) != TYPE_STRING) {
            free(s);
            return create_exception("concat expects strings");
        }

        src_len = strlen(car(args)->value.string);
        len += src_len;
        if (len > sz) {
            if (cdr(args) == NULL) sz = len;
            else while (len > sz)  sz <<= 1;
            s = realloc(s, sz + 1);
        }
        strcpy(s + len - src_len, car(args)->value.string);

        args = cdr(args);
    }

    return create_string_alloced(s);
}

Value *read_file(Value *args, Env *env) {
    FILE *f;
    char *s;
    size_t sz;

    if (car(args) == NULL || TYPEOF(car(args)) != TYPE_STRING || cdr(args) != NULL) {
        return create_exception("read-file expects a single string argument");
    }

    f = fopen(car(args)->value.string, "r");
    if (f == NULL) {
        return create_exception("Cannot open file '%s'", car(args)->value.string);
    }

    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    fclose(f);
    return create_number(create_number_ll(sz));

    // TODO binary rather than string
    s = malloc(sz + 1);
    fread(s, 1, sz, f);
    fclose(f);

    return create_string_alloced(s);
}

Env *create_global_env(void) {
    Env *env = create_env(NULL);

    srand(time(NULL));

    add_to_env(env, "+", create_builtin(bltn_add));
    add_to_env(env, "-", create_builtin(bltn_sub));
    add_to_env(env, "*", create_builtin(bltn_mul));
    add_to_env(env, "/", create_builtin(bltn_div));
    add_to_env(env, "remainder", create_builtin(bltn_rem));
    add_to_env(env, "quote", create_builtin_sf(quote));
    add_to_env(env, "lambda", create_builtin_sf(lambda));
    add_to_env(env, "macro", create_builtin_sf(macro));
    add_to_env(env, "define", create_builtin_sf(define));
    add_to_env(env, "set!", create_builtin_sf(set));
    add_to_env(env, "#t", copy_value(&vtrue));
    add_to_env(env, "#f", copy_value(&vfalse));
    add_to_env(env, "cond", create_builtin_sf(cond));
    add_to_env(env, "=", create_builtin(equal));
    add_to_env(env, "eval", create_builtin(eval_block));
    add_to_env(env, "null?", create_builtin(is_null));
    add_to_env(env, "list?", create_builtin(is_list));
    add_to_env(env, "number?", create_builtin(is_number));
    add_to_env(env, "boolean?", create_builtin(is_boolean));
    add_to_env(env, "exception?", create_builtin(is_exception));
    add_to_env(env, "function?", create_builtin(is_function));
    add_to_env(env, "string?", create_builtin(is_string));
    add_to_env(env, "car", create_builtin(bltn_car));
    add_to_env(env, "cdr", create_builtin(bltn_cdr));
    add_to_env(env, "cons", create_builtin(bltn_cons));
    add_to_env(env, "print", create_builtin(bltn_print));
    add_to_env(env, "try", create_builtin_sf(trycatch));
    add_to_env(env, "<", create_builtin(lt));
    add_to_env(env, ">", create_builtin(gt));
    add_to_env(env, "<=", create_builtin(lte));
    add_to_env(env, ">=", create_builtin(gte));
    add_to_env(env, "print-env", create_builtin(print_env));
    add_to_env(env, "random", create_builtin(bltn_random));
    add_to_env(env, "include", create_builtin(bltn_include));
    add_to_env(env, "raise", create_builtin(bltn_raise));
    add_to_env(env, "string->number", create_builtin(string_to_number));
    add_to_env(env, "number->string", create_builtin(number_to_string));
    add_to_env(env, "concat", create_builtin(concat));
    add_to_env(env, "read-file", create_builtin(read_file));

    return env;
}
