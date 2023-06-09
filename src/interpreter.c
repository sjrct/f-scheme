#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "value.h"
#include "env.h"
#include "builtins.h"
#include "interpreter.h"

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#define FLAG_INTERACTIVE  1
#define FLAG_PRINT_PARSED 2
#define FLAG_NO_STDLIB    4

#define STDLIB_PATH "stdlib.scm"

static Env *global_env;

static Value *parse_value(const char **ptext);
Value *eval(Value *v, Env *env);

static int is_comment(const char *text) {
    return text[0] == ';';
}

static void ignore_whitespace(const char **ptext) {
    while (isspace(**ptext) || is_comment(*ptext)) {
        if (is_comment(*ptext)) {
            while (**ptext != '\n' && **ptext != 0) ++*ptext;
        }

        ++*ptext;
    }
}

static Value *parse_atom(const char **ptext) {
    const char *start = *ptext;

    while (isgraph(**ptext) && **ptext != '(' && **ptext != ')') {
        ++*ptext;
    }

    return create_atom_alloced(strndup(start, *ptext - start));
}

static Value *parse_list(const char **ptext) {
    Value *ls = NULL;
    Value **next = &ls;

    while (1) {
        ignore_whitespace(ptext);
        if (**ptext == ')' || **ptext == 0) {
            ++*ptext;
            break;
        }

        *next = cons(parse_value(ptext), NULL);
        next = &CDR(*next);
    }

    return ls;
}

static void add_char_to_str(char ch, char **pstr, size_t *pstr_len, size_t *pstr_size) {
    if (*pstr_len + 1 >= *pstr_size) {
        *pstr_size <<= 1;
        *pstr = realloc(*pstr, (*pstr_size + 1) * sizeof(char));
    }

    (*pstr)[*pstr_len] = ch;
    ++*pstr_len;
}

static Value *parse_string(const char **ptext) {
    size_t str_len = 0, str_size = 16;
    char *str = malloc((str_size + 1) * sizeof(char));

    ++*ptext;

    while (**ptext != '"' && **ptext) {
        if (**ptext == '\\') {
            switch (*++*ptext) {
            case 'n':
                add_char_to_str('\n', &str, &str_len, &str_size);
                break;
            case 't':
                add_char_to_str('\t', &str, &str_len, &str_size);
                break;
            default:
                add_char_to_str(**ptext, &str, &str_len, &str_size);
                break;
            }
        } else {
            add_char_to_str(**ptext, &str, &str_len, &str_size);
        }
        ++*ptext;
    }

    if (**ptext == '"') ++*ptext;

    str[str_len] = 0;
    return create_string_alloced(str);
}

static Value *parse_value(const char **ptext) {
    Value *v;

    ignore_whitespace(ptext);

    if (isdigit(**ptext) || (**ptext == '-' && isdigit((*ptext)[1]))) {
        v = create_number(parse_number(ptext));
    } else if (**ptext == '(') {
        ++*ptext;
        v = parse_list(ptext);
    } else if (**ptext == '"') {
        v = parse_string(ptext);
    } else if (isgraph(**ptext)) {
        v = parse_atom(ptext);
    } else if (**ptext == EOF) {
        v = NULL;
    } else {
        return create_exception("Unexpected character '%c' (%x)", **ptext, **ptext);
    }

    return v;
}

Value *parse(const char *text) {
    return parse_value(&text);
}

void print(Value *v) {
    switch (TYPEOF(v)) {
    case TYPE_NULL:
        printf("()");
        break;
    case TYPE_ATOM:
        printf("%s", v->value.atom);
        break;
    case TYPE_STRING:
        printf("\"%s\"", v->value.string);
        break;
    case TYPE_NUMBER:
        if (v->value.number.type == NUMBER_LLONG) {
            printf("%lld", v->value.number.v.ll);
        } else {
            printf("%g", v->value.number.v.d);
        }
        break;
    case TYPE_LIST:
        printf("(");

        // TODO fix assumes cdr is LIST
        int first = 1;
        for (Value *it = v; it != NULL; it = it->value.list.cdr) {
            if (first) {
                first = 0;
            } else {
                fputc(' ', stdout);
            }

            print(it->value.list.car);
        }

        printf(")");
        break;
    case TYPE_FUNCTION:
    case TYPE_FUNCTION_SF:
        if (v->type == TYPE_FUNCTION) {
            printf("(lambda ");
        } else {
            printf("(macro ");
        }
        print(v->value.func.operands);
        printf(" ");
        print(v->value.func.body);
        printf(")");
        break;
    case TYPE_BUILTIN:
    case TYPE_BUILTIN_SF:
        printf("[builtin]");
        break;
    case TYPE_BOOLEAN:
        if (v->value.boolean) {
            printf("#t");
        } else {
            printf("#f");
        }
        break;
    case TYPE_EXCEPTION:
    case TYPE_BOUND_EXCEPTION:
        printf("exception: %s", v->value.exception);
        break;
    }
}

static Value *eval_list(Value *v, Env *env) {
    Value *res;
    Value *ls = NULL;
    Value **next = &ls;

    while (v != NULL) {
        res = eval(car(v), env);

        // Bubble exceptions
        if (TYPEOF(res) == TYPE_EXCEPTION) {
            delete_value(ls);
            return res;
        }

        *next = cons(res, NULL);
        next = &CDR(*next);
        v = cdr(v);
    }

    return ls;
}

static int bind_rest_of_args(Value **parg, Value **pparam, Env *frame, int do_eval) {
    // FIXME sloppy sloppy sloppy!!!
    if (TYPEOF(car(cdr(*pparam))) != TYPE_ATOM) {
        return 0;
    }

    const char *name = car(cdr(*pparam))->value.atom;
    *pparam = cdr(cdr(*pparam));

    Value *ls = NULL;
    Value **next = &ls;

    while (*parg != NULL) {
        Value *arg_val = do_eval
            ? eval(car(*parg), frame)
            : copy_value(car(*parg));

        *next = cons(arg_val, NULL);
        next = &CDR(*next);
        *parg = cdr(*parg);
    }

    add_to_env(frame, name, ls);
    return 1;
}

static Value *apply_user_func(Value *func, Value *args, Env *env, int do_eval) {
    assert(IS_FUNCTION(func));

    Env *frame = create_env(env);

    // Bind the arguments in the new stack frame
    Value *arg = args, *param = func->value.func.operands;

    while (arg != NULL && param != NULL) {
        assert(TYPEOF(car(param)) == TYPE_ATOM);

        Value *arg_val = do_eval
            ? eval(car(arg), env)
            : copy_value(car(arg));


        // Bubble exceptions
        if (TYPEOF(arg_val) == TYPE_EXCEPTION) {
            delete_env(frame);
            return arg_val;
        }

        if (!strcmp(car(param)->value.atom, "&rest")) {
            if (!bind_rest_of_args(&arg, &param, frame, do_eval)) {
                delete_env(frame);
                return create_exception("&rest must be followed by name");
            }
            break;
        }

        add_to_env(frame, car(param)->value.atom, arg_val);

        arg = cdr(arg);
        param = cdr(param);
    }

    if (param != NULL || arg != NULL) {
        if (param != NULL && !strcmp(car(param)->value.atom, "&rest")) {
            // special case when 0 args passed to &rest
            bind_rest_of_args(&arg, &param, frame, do_eval);
        } else {
            delete_env(frame);
            return create_exception("argument/parameter mismatch");
        }
    }

    Value *ret = eval(func->value.func.body, frame);
    delete_env(frame);
    return ret;
}

// Doesn't eval arguments
Value *apply_func(Value *func, Value *args, Env *env) {
    if (IS_BUILTIN(args)) {
        return func->value.builtin(args, env);
    } else if (IS_FUNCTION(func)) {
        return apply_user_func(func, args, env, 0);
    }
    return create_exception("Cannot apply value of type %s", type_names[TYPEOF(func)]);
}

Value *eval(Value *v, Env *env) {
    Value *var;
    Value *func;

    //printf("Evaling: ");
    //print(v);
    //puts("");

    switch (TYPEOF(v)) {
        case TYPE_ATOM:
            if (!resolve(env, v->value.atom, &var)) {
                return create_exception("Could not resolve '%s'", v->value.atom);
            }
            return copy_value(var);

        case TYPE_LIST:
            func = eval(car(v), env);

            if (TYPEOF(func) == TYPE_BUILTIN) {
                Value *args = eval_list(cdr(v), env);

                // Bubble exceptions, don't call function
                if (TYPEOF(args) == TYPE_EXCEPTION) return args;

                Value *ret = func->value.builtin(args, env);
                delete_value(args);
                return ret;
            } else if (TYPEOF(func) == TYPE_BUILTIN_SF) {
                return func->value.builtin(cdr(v), env);
            } else if (IS_FUNCTION(func)) {
                Value *args = cdr(v);
                return apply_user_func(func, args, env, func->type == TYPE_FUNCTION);
            } else if (TYPEOF(func) == TYPE_EXCEPTION) {
                return func;
            } else {
                // NOT applyable!
                return create_exception("Cannot apply value of type %s", type_names[TYPEOF(func)]);
            }
            break;

        default:
            return copy_value(v);
    }

    return NULL;
}

Value *eval_block(Value *lines, Env *env) {
    Value *ret = NULL;

    while (lines != NULL) {
        delete_value(ret);
        ret = eval(car(lines), env);
        lines = cdr(lines);
    }

    return ret;
}

Value *run_script(const char *filename, Env *env) {
    FILE *f = fopen(filename, "r");

    if (f == NULL) {
        return create_exception("Cannot open file '%s'.", filename);
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *text = malloc(size + 1);
    text[size] = 0;
    fread(text, 1, size, f);

    fclose(f);

    const char *text2 = text;
    Value *parsed = parse_list(&text2);
    Value *result = eval_block(parsed, env);

    delete_value(parsed);
    free(text);

    return result;
}

static int handle_options(int argc, char **argv, Env *env) {
    int force_interactive = 0;
    int flags = FLAG_INTERACTIVE | FLAG_NO_STDLIB;

    int script_count = 0;
    char *scripts[100];

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'h':
                printf(
                    "%s [OPTIONS]\n"
                    "\n"
                    "Options:\n"
                    "    -h\n"
                    "        Show this help.\n"
                    "    -i\n"
                    "        Enable interactive mode. (default unless script specified)\n"
                    "    -s [script]\n"
                    "        Run the specified script.\n"
                    "    -n\n"
                    "        Do not include the standard library.\n"
                    "    -p\n"
                    "        Print the parsed object in interactive mode.\n"
                    "\n", argv[0]
                );
                exit(0);

            case 'i':
                flags |= FLAG_INTERACTIVE;
                force_interactive = 1;
                break;

            case 's':
                if (i + 1 >= argc) {
                    fprintf(stderr, "Option -s requires a script name\n");
                    exit(EXIT_FAILURE);
                }

                if (!force_interactive) flags &= ~FLAG_INTERACTIVE;

                // FIXME
                if (script_count == 100) {
                    fprintf(stderr, "Error: to many scripts specified.\n");
                    exit(EXIT_FAILURE);
                }

                scripts[script_count] = argv[i + 1];
                script_count += 1;

                break;

            case 'n':
                flags |= FLAG_NO_STDLIB;
                break;

            case 'p':
                flags |= FLAG_PRINT_PARSED;
                break;

            default:
                fprintf(stderr, "Warning: unknown option '%s'\n", argv[i]);
                break;
            }
        }
    }

    if (~flags & FLAG_NO_STDLIB) {
        // FIXME
        delete_value(run_script(STDLIB_PATH, env));
    }

    for (int i = 0; i < script_count; i++) {
        delete_value(run_script(scripts[i], env));
    }

    return flags;
}

#ifdef USE_READLINE
static const char *get_history_path(void) {
    static char *path = NULL;

    if (path == NULL) {
        char *home = getenv("HOME");
        if (home == NULL) {
            path = (char *)".scheme-history";
        } else {
            size_t len = snprintf(NULL, 0, "%s/.scheme-history", home);
            path = malloc(len + 1);
            sprintf(path, "%s/.scheme-history", home);
        }
    }

    return path;
}

static void save_history(void) {
    write_history(get_history_path());
}

// Are we completing a function?
static int completing_function(const char *text, int start) {
    int i = start;
    while (i >= 0) {
        --i;
        if (!isspace(rl_line_buffer[i])) break;
    }
    return rl_line_buffer[i] == '(';
}

static char *global_variable_generator(const char *text, int state, int only_functions) {
    static int len;
    static Env *env;
    static EnvElem *elem;

    EnvElem *curr;

    if (state == 0) {
        env = global_env;
        len = strlen(text);
        elem = env->first;
    }

    while (env != NULL) {
        while (elem != NULL) {
            curr = elem;
            elem = elem->next;

            if ((!only_functions || IS_CALLABLE(curr->value)) && !strncmp(curr->name, text, len)) {
                return strdup(curr->name);
            }
        }

        env = env->parent;
        if (env) elem = env->first;
    }

    return NULL;
}

static char *global_all_variable_generator(const char *text, int state) {
    return global_variable_generator(text, state, 0);
}

static char *global_function_generator(const char *text, int state) {
    return global_variable_generator(text, state, 1);
}

static char **complete_from_global_env(const char * text, int start, int end) {
    if (completing_function(text, start)) {
        return rl_completion_matches(text, global_function_generator);
    } else {
        return rl_completion_matches(text, global_all_variable_generator);
    }
}

static void setup_readline(void) {
    // history
    using_history();
    read_history(get_history_path());
    atexit(save_history);

    // completion
    rl_completer_word_break_characters = " \t\n\"'()";
    rl_attempted_completion_function = complete_from_global_env;
}
#else
// FIXME make better
char *readline(const char *prompt) {
    char *buf = malloc(256);
    fputs(prompt, stdout);
    fgets(buf, 256, stdin);

    if (feof(stdin) && strlen(buf) == 0) {
        return NULL;
    }

    return buf;
}
#endif

int main(int argc, char **argv) {
    global_env = create_global_env();
    int flags = handle_options(argc, argv, global_env);

    if (flags & FLAG_INTERACTIVE) {
#ifdef USE_READLINE
        setup_readline();
#endif

        while (!feof(stdin)) {
            char *input = readline("> ");
            if (!input) break;

            Value *parsed = parse(input);
            Value *result = eval(parsed, global_env);

            if (flags & FLAG_PRINT_PARSED) {
                printf("%% ");
                print(parsed);
                puts("");
            }

            if (result) {
                print(result);
                puts("");
                delete_value(result);
            }

            delete_value(parsed);

#ifdef USE_READLINE
            if (input[0]) add_history(input);
#endif
            free(input);
        }
    }

    return 0;
}
