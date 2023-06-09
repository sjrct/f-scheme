#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "value.h"

Value *eval(Value *v, Env *env);
Value *eval_block(Value *v, Env *env);

void print(Value *);

#endif
