// Compilation logic for expressions
#include <assert.h>
#include <libgccjit.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>

#include "../ast.h"
#include "../parse.h"
#include "../span.h"
#include "../typecheck.h"
#include "../types.h"
#include "../units.h"
#include "../util.h"
#include "../SipHash/halfsiphash.h"
#include "compile.h"
#include "libgccjit_abbrev.h"

match_outcomes_t perform_conditional_match(env_t *env, gcc_block_t **block, sss_type_t *t, gcc_rvalue_t *val, ast_t *pattern)
{
    gcc_loc_t *loc = ast_loc(env, pattern);
    gcc_func_t *func = gcc_block_func(*block);

    match_outcomes_t outcomes = {
        .match_block = gcc_new_block(func, fresh("match_success")),
        .no_match_block = gcc_new_block(func, fresh("match_failure")),
        .match_env = fresh_scope(env),
    };

    switch (pattern->tag) {
    case Var: {
        const char *name = Match(pattern, Var)->name;
        if (t->tag == TaggedUnionType) {
            auto tu_t = Match(t, TaggedUnionType);
            foreach (tu_t->members, member, _) {
                if (streq(member->name, name)) {
                    gcc_struct_t *tagged_struct = gcc_type_if_struct(sss_type_to_gcc(env, t));
                    gcc_rvalue_t *is_match = gcc_comparison(
                        env->ctx, NULL, GCC_COMPARISON_EQ,
                        gcc_rvalue_access_field(val, loc, gcc_get_field(tagged_struct, 0)),
                        gcc_rvalue_from_long(env->ctx, get_tag_type(env, t), member->tag_value));
                    gcc_jump_condition(*block, loc, is_match, outcomes.match_block, outcomes.no_match_block);
                    *block = NULL;
                    return outcomes;
                }
            }
        }

        binding_t *b = get_binding(env, name);
        if (!b) {
            if (!streq(name, "*"))
                hset(outcomes.match_env->bindings, name, new(binding_t, .type=t, .rval=val));
            gcc_jump(*block, loc, outcomes.match_block);
            *block = NULL;
            return outcomes;
        }
        goto compare_values;
    }
    case HeapAllocate: {
        if (t->tag != PointerType)
            compiler_err(env, pattern, "This is a pointer pattern, but you're attempting to match it against a non-pointer value with type %T", t);

        if (Match(t, PointerType)->is_optional) {
            gcc_rvalue_t *is_nonnull = gcc_comparison(
                env->ctx, NULL, GCC_COMPARISON_NE, val, gcc_null(env->ctx, sss_type_to_gcc(env, t)));

            gcc_jump_condition(*block, loc, is_nonnull, outcomes.match_block, outcomes.no_match_block);
        } else {
            gcc_jump(*block, loc, outcomes.match_block);
        }
        *block = NULL;
        gcc_rvalue_t *data_val = gcc_rval(gcc_rvalue_dereference(val, loc));
        ast_t *pointed = Match(pattern, HeapAllocate)->value;
        auto submatch_outcomes = perform_conditional_match(env, &outcomes.match_block, Match(t, PointerType)->pointed, data_val, pointed);
        gcc_jump(submatch_outcomes.no_match_block, loc, outcomes.no_match_block);
        return (match_outcomes_t){
            submatch_outcomes.match_block,
            outcomes.no_match_block,
            submatch_outcomes.match_env,
        };
    }
    case Struct: {
        auto pat_struct = Match(pattern, Struct);
        if (t->tag != StructType) {
            compiler_err(env, pattern, "This is a struct pattern, but you're attempting to match it against a non-struct value with type %T", t);
        } else if (pat_struct->type) {
            sss_type_t *pat_t = get_type(env, pat_struct->type);
            if (pat_t->tag != TypeType)
                compiler_err(env, pat_struct->type, "This is not a valid struct type");
            pat_t = Match(pat_t, TypeType)->type;
            if (!type_eq(t, pat_t))
                compiler_err(env, pattern, "This pattern is a %T, but you're attempting to match it against a value with type %T", pat_t, t);
        } else if (Match(t, StructType)->name) {
            compiler_err(env, pattern, "This pattern is a nameless tuple, but you're attempting to match it against a value with type %T", t);
        } else if (!streq(Match(t, StructType)->units, pat_struct->units)) {
            compiler_err(env, pattern, "The units of this pattern: <%s> don't match the units of the value being matched: <%s>",
                         Match(t, StructType)->units, pat_struct->units);
        }

        auto struct_info = Match(t, StructType);
        gcc_struct_t *gcc_struct = gcc_type_if_struct(sss_type_to_gcc(env, t));
        sss_hashmap_t field_types = {0};
        for (int64_t i = 0; i < length(struct_info->field_names); i++) {
            const char *name = ith(struct_info->field_names, i); 
            sss_type_t *type = ith(struct_info->field_types, i); 
            hset(&field_types, name, type);
        }

        gcc_jump(*block, loc, outcomes.match_block);
        *block = NULL;

        sss_hashmap_t field_pats = {0};
        // Named fields:
        for (int64_t i = 0; i < LIST_LEN(pat_struct->members); i++) {
            ast_t *field_ast = ith(pat_struct->members, i);
            const char *name = field_ast->tag == KeywordArg ? Match(field_ast, KeywordArg)->name : NULL;
            if (!name) continue;
            if (hget(&field_pats, name, ast_t*))
                compiler_err(env, field_ast, "This struct member is a duplicate of an earlier member.");
            else if (!hget(&field_types, name, sss_type_t*))
                compiler_err(env, field_ast, "This is not a valid member of the struct %T", t);

            ast_t *pat_member = field_ast->tag == KeywordArg ? Match(field_ast, KeywordArg)->arg : field_ast;
            hset(&field_pats, name, pat_member);
        }
        // Unnamed fields:
        for (int64_t i = 0; i < LIST_LEN(pat_struct->members); i++) {
            ast_t *field_ast = ith(pat_struct->members, i);
            const char *name = field_ast->tag == KeywordArg ? Match(field_ast, KeywordArg)->name : NULL;
            if (name) continue;
            ast_t *pat_member = field_ast->tag == KeywordArg ? Match(field_ast, KeywordArg)->arg : field_ast;
            foreach (struct_info->field_names, name, _) {
                if (!hget(&field_pats, (*name), ast_t*)) {
                    hset(&field_pats, (*name), pat_member);
                    goto found_name;
                }
            }
            compiler_err(env, field_ast, "This is one field too many for this struct");
          found_name: continue;
        }

        for (int64_t i = 0; i < LIST_LEN(struct_info->field_names); i++) {
            const char *name = ith(struct_info->field_names, i);
            auto pat = hget(&field_pats, name, ast_t*);
            if (!pat) continue;
            gcc_rvalue_t *member_val = gcc_rvalue_access_field(val, loc, gcc_get_field(gcc_struct, i));
            auto submatch_outcomes = perform_conditional_match(outcomes.match_env, &outcomes.match_block, ith(struct_info->field_types, i), member_val, pat);
            outcomes.match_block = submatch_outcomes.match_block;
            outcomes.match_env = submatch_outcomes.match_env;
            gcc_jump(submatch_outcomes.no_match_block, loc, outcomes.no_match_block);
        }
        return outcomes;
    }
    case FunctionCall: {
        auto call = Match(pattern, FunctionCall);
        if (call->fn->tag != Var)
            goto compare_values;

        const char *fn_name = Match(call->fn, Var)->name;
        if (t->tag != TaggedUnionType)
            goto compare_values;

        // Tagged Union Constructor:
        auto tu_t = Match(t, TaggedUnionType);
        int64_t tag_index = -1;
        for (int64_t i = 0; i < LIST_LEN(tu_t->members); i++) {
            if (streq(ith(tu_t->members, i).name, fn_name)) {
                tag_index = i;
                goto found_tag;
            }
        }
        // Failed to find the tag, this is just a function call:
        goto compare_values;

      found_tag:;
        auto member = ith(tu_t->members, tag_index);
        if (!member.type)
            compiler_err(env, pattern, "This tagged union member doesn't have any value");

        gcc_struct_t *tagged_struct = gcc_type_if_struct(sss_type_to_gcc(env, t));

        gcc_type_t *tag_gcc_t = get_tag_type(env, t);
        gcc_type_t *union_gcc_t = get_union_type(env, t);
        gcc_field_t *tag_field = gcc_get_field(tagged_struct, 0);
        gcc_field_t *data_field = gcc_get_field(tagged_struct, 1);

        gcc_rvalue_t *is_match = gcc_comparison(
            env->ctx, NULL, GCC_COMPARISON_EQ,
            gcc_rvalue_access_field(val, loc, tag_field),
            gcc_rvalue_from_long(env->ctx, tag_gcc_t, member.tag_value));

        gcc_jump_condition(*block, loc, is_match, outcomes.match_block, outcomes.no_match_block);
        *block = NULL;

        gcc_rvalue_t *data_val = gcc_rvalue_access_field(
            gcc_rvalue_access_field(val, loc, data_field), loc, gcc_get_union_field(union_gcc_t, tag_index));
        ast_t *m_pat = WrapAST(pattern, Struct, .members=call->args);
        auto submatch_outcomes = perform_conditional_match(env, &outcomes.match_block, member.type, data_val, m_pat);
        gcc_jump(submatch_outcomes.no_match_block, loc, outcomes.no_match_block);
        return (match_outcomes_t){
            submatch_outcomes.match_block,
            outcomes.no_match_block,
            submatch_outcomes.match_env,
        };
    }
    default: {
      compare_values:;
        sss_type_t *pattern_t = get_type(env, pattern);
        gcc_rvalue_t *pattern_val = compile_expr(env, block, pattern);
        if (!promote(env, pattern_t, &pattern_val, t))
            compiler_err(env, pattern, "This pattern has type %T, but you're attempting to match it against a value with type %T", pattern_t, t);
        gcc_rvalue_t *is_match;
        if (is_numeric(t) || t->tag == BoolType || t->tag == CharType || t->tag == PointerType)
            is_match = gcc_comparison(env->ctx, loc, GCC_COMPARISON_EQ, val, pattern_val);
        else
            is_match = gcc_comparison(env->ctx, loc, GCC_COMPARISON_EQ, compare_values(env, t, val, pattern_val),
                                      gcc_zero(env->ctx, gcc_type(env->ctx, INT)));

        gcc_jump_condition(*block, loc, is_match, outcomes.match_block, outcomes.no_match_block);
        *block = NULL;
        return outcomes;
    }
    }
}

// vim: ts=4 sw=0 et cino=L2,l1,(0,W4,m1,\:0
