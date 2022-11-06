// Logic for compiling Blang blocks
#include <assert.h>
#include <bhash.h>
#include <libgccjit.h>
#include <bp/files.h>
#include <ctype.h>
#include <err.h>
#include <gc/cord.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>

#include "../ast.h"
#include "compile.h"
#include "libgccjit_abbrev.h"
#include "../typecheck.h"
#include "../types.h"
#include "../util.h"

void compile_statement(env_t *env, gcc_block_t **block, ast_t *ast)
{
    check_discardable(env->file, env->bindings, ast);
    gcc_rvalue_t *val = compile_expr(env, block, ast);
    if (val)
        gcc_eval(*block, ast_loc(env, ast), val);
}

gcc_rvalue_t *compile_block(env_t *env, gcc_block_t **block, ast_t *ast, bool return_value)
{
    // Function defs are visible in the entire block (allowing corecursive funcs)
    foreach (ast->children, stmt, last_stmt) {
        if ((*stmt)->kind == FunctionDef) {
            bl_type_t *t = get_type(env->file, env->bindings, *stmt);
            gcc_func_t *func = get_function_def(env, *stmt, false);
            gcc_rvalue_t *fn_ptr = gcc_get_func_address(func, NULL);
            hashmap_set(env->bindings, (*stmt)->fn.name,
                        new(binding_t, .type=t, .is_global=true, .func=func, .rval=fn_ptr));
        }
    }

    foreach (ast->children, stmt, last_stmt) {
        // Declarations are visible from here onwards:
        if ((*stmt)->kind == FunctionDef) {
            binding_t *binding = hashmap_get(env->bindings, (*stmt)->fn.name);
            assert(binding);
            // Compile the function here instead of above because we need the type information
            // from the other functions in the block.
            compile_function(env, binding->func, *stmt);
        }
        if (stmt == last_stmt && return_value) {
            return compile_expr(env, block, *stmt);
        } else {
            compile_statement(env, block, *stmt);
        }
    }
    return NULL;
}


// vim: ts=4 sw=0 et cino=L2,l1,(0,W4,m1,\:0
