#ifndef ENV_H
#define ENV_H

struct Env;
struct EnvElem;
typedef struct Env Env;
typedef struct EnvElem EnvElem;

#include "value.h"

struct EnvElem {
    char *name;
    Value *value;
    EnvElem *next;
};

struct Env {
    int refs;
    EnvElem *first;
    Env *parent;
};

Env *create_env(Env *parent);
Env *copy_env(Env *env);
void delete_env(Env *env);

// Do NOT increment the ref counter
void add_to_env(Env *env, const char *name, Value *v);
void set_in_env(Env *env, const char *name, Value *v);

int resolve(Env *env, char *name, Value **dst);

#endif
