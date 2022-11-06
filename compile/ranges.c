// Logic for compiling Blang ranges (`1..10`)
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

gcc_rvalue_t *compile_range(env_t *env, gcc_block_t **block, ast_t *ast)
{
    gcc_type_t *range_t = bl_type_to_gcc(env, Type(RangeType));
    gcc_struct_t *range_struct = gcc_type_if_struct(range_t);
    assert(range_struct);
    gcc_rvalue_t *values[] = {
        ast->range.first ? compile_expr(env, block, ast->range.first) : gcc_int64(env->ctx, INT64_MIN),
        ast->range.step ? compile_expr(env, block, ast->range.step) : gcc_int64(env->ctx, 1),
        ast->range.last ? compile_expr(env, block, ast->range.last) : gcc_int64(env->ctx, INT64_MAX),
    };
    return gcc_struct_constructor(env->ctx, NULL, range_t, 3, NULL, values);
}

void compile_range_iteration(env_t *env, gcc_block_t **block, ast_t *ast, block_compiler_t body_compiler, void *userdata)
{

    gcc_func_t *func = gcc_block_func(*block);
    gcc_block_t *loop_body = gcc_new_block(func, NULL),
                *loop_between = ast->for_loop.between ? gcc_new_block(func, NULL) : NULL,
                *loop_next = gcc_new_block(func, NULL),
                *loop_end = gcc_new_block(func, NULL);

    env_t loop_env = *env;
    loop_env.bindings = hashmap_new();
    loop_env.bindings->fallback = env->bindings;
    loop_env.loop_label = &(loop_label_t){
        .enclosing = env->loop_label,
        .name = intern_str("for"),
        .skip_label = loop_next,
        .stop_label = loop_end,
    };
    env = &loop_env;

    // Preamble:
    bl_type_t *range_t = get_type(env->file, env->bindings, ast->for_loop.iter);
    assert(range_t->kind == RangeType);
    gcc_rvalue_t *range = compile_expr(env, block, ast->for_loop.iter);
    gcc_type_t *gcc_range_t = bl_type_to_gcc(env, range_t);
    gcc_type_t *i64_t = gcc_type(env->ctx, INT64);

    // val = range.first
    gcc_struct_t *range_struct = gcc_type_if_struct(gcc_range_t);
    assert(range_struct);
    gcc_lvalue_t *val;
    if (ast->for_loop.value) {
        if (ast->for_loop.value->kind != Var)
            ERROR(env, ast->for_loop.value, "This needs to be a variable");
        val = gcc_local(func, ast_loc(env, ast->for_loop.value), i64_t, fresh(ast->for_loop.value->str));
        hashmap_set(env->bindings, ast->for_loop.value->str, new(binding_t, .rval=gcc_lvalue_as_rvalue(val), .type=Type(IntType)));
    } else {
        val = gcc_local(func, NULL, i64_t, fresh("val"));
    }
    gcc_assign(*block, NULL, val,
               gcc_rvalue_access_field(range, NULL, gcc_get_field(range_struct, 0)));

    // step = range.step
    gcc_lvalue_t *step = gcc_local(func, NULL, i64_t, fresh("step"));
    gcc_assign(*block, NULL, step,
               gcc_rvalue_access_field(range, NULL, gcc_get_field(range_struct, 1)));

    // sign = step / abs(step)
    gcc_lvalue_t *sign = gcc_local(func, NULL, i64_t, fresh("sign"));
    gcc_assign(*block, NULL, sign,
               gcc_binary_op(env->ctx, NULL, GCC_BINOP_DIVIDE, i64_t,
                             gcc_lvalue_as_rvalue(step),
                             gcc_unary_op(env->ctx, NULL, GCC_UNOP_ABS, i64_t, gcc_lvalue_as_rvalue(step))));

    // last = range.last
    gcc_lvalue_t *last = gcc_local(func, NULL, i64_t, fresh("last"));
    gcc_assign(*block, NULL, last,
               gcc_rvalue_access_field(range, NULL, gcc_get_field(range_struct, 2)));

    // index = 1
    gcc_rvalue_t *one64 = gcc_one(env->ctx, gcc_type(env->ctx, INT64));
    gcc_lvalue_t *index_var = NULL;
    if (ast->for_loop.key) {
        index_var = gcc_local(func, ast_loc(env, ast->for_loop.key), i64_t, fresh(ast->for_loop.key->str));
        if (ast->for_loop.key->kind != Var)
            ERROR(env, ast->for_loop.key, "This needs to be a variable");
        hashmap_set(env->bindings, ast->for_loop.key->str, new(binding_t, .rval=gcc_lvalue_as_rvalue(index_var), .type=Type(IntType)));
        gcc_assign(*block, NULL, index_var, one64);
    }

    // goto ((last - val)*sign < 0) ? body : end
    gcc_rvalue_t *is_done = gcc_comparison(
        env->ctx, NULL, GCC_COMPARISON_LT,
        gcc_binary_op(env->ctx, NULL, GCC_BINOP_MULT, i64_t,
                      gcc_binary_op(env->ctx, NULL, GCC_BINOP_MINUS, i64_t,
                                    gcc_lvalue_as_rvalue(last),
                                    gcc_lvalue_as_rvalue(val)),
                      gcc_lvalue_as_rvalue(sign)),
        gcc_zero(env->ctx, i64_t));
    gcc_jump_condition(*block, NULL, is_done, loop_end, loop_body);
    *block = NULL;

    // body:
    gcc_block_t *loop_body_end = loop_body;

    // body block
    if (body_compiler)
        body_compiler(env, &loop_body_end, ast->for_loop.body, userdata);
    else
        (void)compile_block(env, &loop_body_end, ast->for_loop.body, false);

    // next:
    // index++, val+=step
    gcc_update(loop_next, NULL, index_var, GCC_BINOP_PLUS, one64);
    gcc_update(loop_next, NULL, val, GCC_BINOP_PLUS, gcc_lvalue_as_rvalue(step));
    if (loop_between) {
        // goto is_done ? end : between
        gcc_jump_condition(loop_next, NULL, is_done, loop_end, loop_between);
        // between:
        (void)compile_block(env, &loop_between, ast->for_loop.between, false);
        if (loop_between)
            gcc_jump(loop_between, NULL, loop_body); // goto body
    } else {
        // goto body
        gcc_jump_condition(loop_next, NULL, is_done, loop_end, loop_body);
    }

    *block = loop_end;
}
