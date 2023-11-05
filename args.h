// Common logic used in typechecking and compiling for parsing a list of
// arguments (or keyword arguments) into their appropriate bindings, given a
// specification

#include <stdbool.h>
#include <stdint.h>

#include "ast.h"
#include "environment.h"
#include "builtins/array.h"

typedef struct {
    ast_t *ast;
    sss_type_t *type;
    const char *name;
    int64_t position;
    bool is_default;
} arg_info_t;

ARRAY_OF(arg_info_t) bind_arguments(env_t *env, ARRAY_OF(ast_t*) args, ARRAY_OF(const char*) arg_names, ARRAY_OF(sss_type_t*) arg_types, ARRAY_OF(ast_t*) arg_defaults);

// vim: ts=4 sw=0 et cino=L2,l1,(0,W4,m1,\:0
