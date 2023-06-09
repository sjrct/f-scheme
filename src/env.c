//#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include "env.h"

Env *create_env(Env *parent) {
    Env *env = malloc(sizeof *env);
    env->first = NULL;
    env->parent = parent;
    env->refs = 1;

    if (parent != NULL) parent->refs += 1;
    return env;
}

Env *copy_env(Env *env) {
    env->refs += 1;
    return env;
}

void delete_env(Env *env) {
    if (!--env->refs) {
        EnvElem *next;
        for (EnvElem *v = env->first; v != NULL; v = next) {
            if (delete_value(v->value)) {
                // There are still other people holding onto this
                track_value(v->value);
            }

            free(v->name);
            next = v->next;
            free(v);
        }
    }
}

static EnvElem *find_item(Env *env, const char *name) {
    for (EnvElem *it = env->first; it != NULL; it = it->next) {
        if (!strcmp(name, it->name)) {
            return it;
        }
    }
    return NULL;
}

static void insert(Env *env, EnvElem *elem, const char *name, Value *v) {
    if (elem == NULL) {
        elem = malloc(sizeof *elem);
        elem->name = strdup(name);
        elem->next = env->first;
        env->first = elem;
    } else {
        delete_value(elem->value);
    }
    untrack_value(v); // Don't GC track values stored in an environment
    elem->value = v;
}


// Expects the ref counter to already be incremented
void add_to_env(Env *env, const char *name, Value *v) {
    EnvElem *elem = find_item(env, name);
    insert(env, elem, name, v);
}

// like add, but will also search parent environments
void set_in_env(Env *env, const char *name, Value *v) {
    Env *search_env = env;
    EnvElem *elem = NULL;

    while (elem == NULL && search_env != NULL) {
        elem = find_item(search_env, name);
        search_env = search_env->parent;
    }

    insert(env, elem, name, v);
}

int resolve(Env *env, char *name, Value **dst) {
    EnvElem *item = find_item(env, name);
    if (item != NULL) {
        *dst = item->value;
        return 1;
    }

    if (env->parent == NULL) {
        *dst = NULL;
        return 0;
    } else {
        return resolve(env->parent, name, dst);
    }
}
