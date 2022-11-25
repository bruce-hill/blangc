// Logic for getting a Blang type from an AST node
#include <bhash.h>
#include <gc.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "typecheck.h"
#include "types.h"
#include "util.h"

#define TYPE_ERR(f, ast, fmt, ...) do { \
    fprintf(stderr, "\x1b[31;7;1m" fmt "\x1b[m\n\n" __VA_OPT__(,) __VA_ARGS__); \
    highlight_match(stderr, f, (ast)->match, 2); \
    exit(1); } while (0)

static bl_type_t *get_clause_type(file_t *f, hashmap_t *bindings, ast_t *condition, ast_t *body)
{
    if (condition && condition->kind == Declare) {
        hashmap_t *body_bindings = hashmap_new();
        body_bindings->fallback = bindings;
        bl_type_t *t = get_type(f, bindings, condition);
        assert(t);
        binding_t b = {.type=t};
        hashmap_set(body_bindings, condition->lhs->str, &b);
        return get_type(f, body_bindings, body);
    } else {
        return get_type(f, bindings, body);
    }
}

static bl_type_t *parse_type(file_t *f, hashmap_t *bindings, ast_t *ast)
{
    switch (ast->kind) {
    case TypeName: {
        binding_t *binding = hashmap_get(bindings, ast->str);
        if (!binding) {
            TYPE_ERR(f, ast, "This type name is not defined");
        }
        return binding->type->type;
    }

    case TypeList: {
        bl_type_t *item_t = parse_type(f, bindings, ast->child);
        if (!item_t) TYPE_ERR(f, ast->child, "This item type is not defined");
        return Type(ListType, .item_type=item_t);
    }

    case TypeOption: {
        bl_type_t *item_t = parse_type(f, bindings, ast->child);
        if (!item_t) TYPE_ERR(f, ast->child, "This option type is not defined");
        return Type(OptionalType, .nonnil=item_t);
    }
    case TypeFunction: {
        bl_type_t *ret_t = parse_type(f, bindings, ast->fn.ret_type);
        NEW_LIST(bl_type_t*, arg_types);
        LIST_FOR (ast->fn.arg_types, arg_t, _) {
            bl_type_t *bl_arg_t = parse_type(f, bindings, *arg_t);
            APPEND(arg_types, bl_arg_t);
        }
        return Type(FunctionType, .args=arg_types, .ret=ret_t);
    }
    default: TYPE_ERR(f, ast, "This is not a Type value");
    }
}

bl_type_t *get_type(file_t *f, hashmap_t *bindings, ast_t *ast)
{
    switch (ast->kind) {
        case Nil: {
            bl_type_t *nonnil = parse_type(f, bindings, ast->child);
            return nonnil->kind == OptionalType ? nonnil : Type(OptionalType, .nonnil=nonnil);
        }
        case Bool: {
            return Type(BoolType);
        }
        case Int: {
            return Type(IntType);
        }
        case Num: {
            return Type(NumType);
        }
        case TypeOf: {
            bl_type_t *t = get_type(f, bindings, ast->child);
            return Type(TypeType, .type=t);
        }
        case SizeOf: {
            return Type(IntType);
        }
        case Maybe: {
            bl_type_t *nonnil = get_type(f, bindings, ast->child);
            return nonnil->kind == OptionalType ? nonnil : Type(OptionalType, .nonnil=nonnil);
        }
        case Range: {
            return Type(RangeType);
        }
        case StringJoin: case StringLiteral: {
            return Type(StringType);
        }
        case Var: {
            binding_t *binding = hashmap_get(bindings, ast->str);
            if (binding) {
                return binding->type;
            } else {
                TYPE_ERR(f, ast, "Couldn't figure out what type %s refers to", ast->str);
            }
        }
        case Len: {
            return Type(IntType);
        }
        case List: {
            if (ast->list.type)
                return parse_type(f, bindings, ast->list.type);

            bl_type_t *item_type = NULL;
            for (int64_t i = 0; i < LIST_LEN(ast->list.items); i++) {
                ast_t *item = LIST_ITEM(ast->list.items, i);
                bl_type_t *t2;
                switch (item->kind) {
                case For: {
                    bl_type_t *iter_t = get_type(f, bindings, item->for_loop.iter);
                    if (iter_t->kind == OptionalType) iter_t = iter_t->nonnil;
                    hashmap_t *loop_bindings = hashmap_new();
                    loop_bindings->fallback = bindings;
                    switch (iter_t->kind) {
                    case ListType:
                        if (item->for_loop.key)
                            hashmap_set(loop_bindings, item->for_loop.key->str, new(binding_t, .type=Type(IntType)));
                        if (item->for_loop.value)
                            hashmap_set(loop_bindings, item->for_loop.value->str, new(binding_t, .type=iter_t->item_type));
                        break;
                    case RangeType:
                        if (item->for_loop.key)
                            hashmap_set(loop_bindings, item->for_loop.key->str, new(binding_t, .type=Type(IntType)));
                        if (item->for_loop.value)
                            hashmap_set(loop_bindings, item->for_loop.value->str, new(binding_t, .type=Type(IntType)));
                        break;
                    default:
                        TYPE_ERR(f, item->for_loop.iter, "This value has type %s, which is not iterable", type_to_string(iter_t));
                        break;
                    }
                    t2 = get_type(f, loop_bindings, item->for_loop.body);
                    break;
                }
                case While: case Repeat: {
                    t2 = get_type(f, bindings, item->loop.body);
                    break;
                }
                case If: {
                    TYPE_ERR(f, item, "Conditional items in lists not yet supported");
                }
                default: {
                    t2 = get_type(f, bindings, item);
                    break;
                }
                }
                bl_type_t *merged = item_type ? type_or_type(item_type, t2) : t2;
                if (!merged)
                    TYPE_ERR(f, LIST_ITEM(ast->list.items, i),
                             "This list item has type %s, which is different from earlier items which have type %s",
                             type_to_string(t2),  type_to_string(item_type));
                item_type = merged;
            }
            return Type(ListType, .item_type=item_type);
        }
        case Index: {
            bl_type_t *indexed_t = get_type(f, bindings, ast->indexed);
            // TODO: support methods
            switch (indexed_t->kind) {
            case ListType: {
                bl_type_t *index_t = get_type(f, bindings, ast->index);
                switch (index_t->kind) {
                case IntType: case Int32Type: case Int16Type: case Int8Type: break;
                default: TYPE_ERR(f, ast->index, "This value is not an integer");
                }
                return indexed_t->item_type;
            }
            case StructType: {
                // TODO: support accessing fields by integer
                assert(ast->index->kind == FieldName);
                for (int64_t i = 0, len = LIST_LEN(indexed_t->struct_.field_names); i < len; i++) {
                    if (LIST_ITEM(indexed_t->struct_.field_names, i) == ast->index->str)
                        return LIST_ITEM(indexed_t->struct_.field_types, i);
                }
                for (int64_t i = 0, len = LIST_LEN(indexed_t->struct_.method_names); i < len; i++) {
                    if (LIST_ITEM(indexed_t->struct_.method_names, i) == ast->index->str)
                        return LIST_ITEM(indexed_t->struct_.method_types, i);
                }
                TYPE_ERR(f, ast->index, "This is not a valid member of type %s", type_to_string(indexed_t));
            }
            case TypeType: {
                switch (indexed_t->type->kind) {
                case StructType: {
                    for (int64_t i = 0, len = LIST_LEN(indexed_t->type->struct_.method_names); i < len; i++) {
                        if (LIST_ITEM(indexed_t->type->struct_.method_names, i) == ast->index->str)
                            return LIST_ITEM(indexed_t->type->struct_.method_types, i);
                    }
                    TYPE_ERR(f, ast->index, "This is not a valid member of type %s", type_to_string(indexed_t->type));
                }
                default: {
                    TYPE_ERR(f, ast, "Static methods are not supported for type %s", type_to_string(indexed_t->type));
                }
                }
            }
            default: {
                TYPE_ERR(f, ast, "Indexing is not supported for type %s", type_to_string(indexed_t));
            }
            // TODO: support static methods
            }
            // TODO: index ranges
        }
        case KeywordArg: {
            return get_type(f, bindings, ast->named.value);
        }
        case FunctionCall: {
            bl_type_t *fn_type = get_type(f, bindings, ast->call.fn);
            if (fn_type->kind != FunctionType) {
                TYPE_ERR(f, ast->call.fn, "This function is a %s and not a function", type_to_string(fn_type));
            }
            int64_t max_args = LIST_LEN(fn_type->args);
            int64_t min_args = max_args;
            while (min_args > 0 && LIST_ITEM(fn_type->args, min_args-1)->kind == OptionalType)
                --min_args;
            int64_t len_args = LIST_LEN(ast->call.args);
            int64_t num_selfs = 0;
            if (ast->call.fn->kind == Index) {
                // Insert "self" argument
                ast_t *self = ast->call.fn->indexed;
                bl_type_t *self_t = get_type(f, bindings, self);
                if (self_t->kind != TypeType) {
                    bl_type_t *expected = LIST_ITEM(fn_type->args, 0);
                    if (is_numeric(self_t) && is_numeric(expected) && numtype_priority(self_t) < numtype_priority(expected))
                        self_t = expected;
                    if (!type_is_a(self_t, expected)) {
                        TYPE_ERR(f, self, "This argument has the wrong type. Expected %s but got %s",
                                 type_to_string(expected), type_to_string(self_t));
                    }
                    num_selfs += 1;
                }
            }
            if (num_selfs + len_args < min_args) {
                TYPE_ERR(f, ast, "Expected to get at least %ld arguments but only got %ld", min_args, num_selfs + len_args);
            } else if (num_selfs + len_args > max_args) {
                TYPE_ERR(f, LIST_ITEM(ast->call.args, max_args), "Too many arguments provided to this function call. Everything from here on is too much.");
            }
            for (int64_t i = 0; i < len_args; i++) {
                ast_t *arg = LIST_ITEM(ast->call.args, i);
                bl_type_t *arg_t = get_type(f, bindings, arg);
                bl_type_t *expected = LIST_ITEM(fn_type->args, num_selfs + i);
                if (is_numeric(arg_t) && is_numeric(expected) && numtype_priority(arg_t) < numtype_priority(expected))
                    arg_t = expected;
                if (!type_is_a(arg_t, expected)) {
                    TYPE_ERR(f, arg, "This argument has the wrong type. Expected %s but got %s",
                             type_to_string(expected), type_to_string(arg_t));
                }
            }
            return fn_type->ret;
        }
        case Block: {
            ast_t *last = LIST_ITEM(ast->children, LIST_LEN(ast->children)-1);
            return get_type(f, bindings, last);
        }
        case Declare: {
            return get_type(f, bindings, ast->rhs);
        }
        case Extern: {
            return Type(VoidType);
        }
        case Assign: {
            return Type(VoidType);
        }
        case Return: case Fail: case Stop: case Skip: {
            return Type(AbortType);
        }
        case Cast: case As: {
            return parse_type(f, bindings, ast->type);
        }
        case TypeName: case TypeList: case TypeOption: case TypeFunction: {
            bl_type_t *t = parse_type(f, bindings, ast);
            return Type(TypeType, .type=t);
        }
        case Negative: {
            bl_type_t *t = get_type(f, bindings, ast->child);
            if (!is_numeric(t))
                TYPE_ERR(f, ast, "Negation is only supported for numeric types, not %s", type_to_string(t));
            return t;
        }
        case And: {
            bl_type_t *lhs_t = get_type(f, bindings, ast->lhs),
                      *rhs_t = get_type(f, bindings, ast->rhs);

            if (lhs_t->kind == BoolType && rhs_t->kind == BoolType)
                return lhs_t;
            else if (is_integral(lhs_t) && is_integral(rhs_t))
                return numtype_priority(lhs_t) >= numtype_priority(rhs_t) ? lhs_t : rhs_t;

            TYPE_ERR(f, ast, "The left and right sides of this 'and' are not compatible: %s and %s",
                     type_to_string(lhs_t), type_to_string(rhs_t));
        }
        case Or: {
            bl_type_t *lhs_t = get_type(f, bindings, ast->lhs),
                      *rhs_t = get_type(f, bindings, ast->rhs);

            if (lhs_t->kind == BoolType && rhs_t->kind == BoolType)
                return lhs_t;
            else if (is_integral(lhs_t) && is_integral(rhs_t))
                return numtype_priority(lhs_t) >= numtype_priority(rhs_t) ? lhs_t : rhs_t;

            if (lhs_t->kind == OptionalType) {
                if (rhs_t == lhs_t->nonnil || rhs_t->kind == AbortType)
                    return lhs_t->nonnil;
                else if (rhs_t == lhs_t)
                    return lhs_t;
            }
            TYPE_ERR(f, ast, "The left and right sides of this 'or' are not compatible: %s and %s",
                     type_to_string(lhs_t), type_to_string(rhs_t));
        }
        case Xor: {
            bl_type_t *lhs_t = get_type(f, bindings, ast->lhs),
                      *rhs_t = get_type(f, bindings, ast->rhs);

            if (lhs_t->kind == BoolType && rhs_t->kind == BoolType)
                return lhs_t;
            else if (is_integral(lhs_t) && is_integral(rhs_t))
                return numtype_priority(lhs_t) >= numtype_priority(rhs_t) ? lhs_t : rhs_t;

            TYPE_ERR(f, ast, "The left and right sides of this 'xor' are not compatible: %s and %s",
                     type_to_string(lhs_t), type_to_string(rhs_t));
        }
        case AddUpdate: case SubtractUpdate: case DivideUpdate: case MultiplyUpdate:
        case Add: case Subtract: case Divide: case Multiply: case Power: case Modulus: {
            bl_type_t *t1 = get_type(f, bindings, ast->lhs),
                      *t2 = get_type(f, bindings, ast->rhs);

            if (AddUpdate <= ast->kind && ast->kind <= DivideUpdate) {
                if (is_numeric(t1) && is_numeric(t2) && numtype_priority(t1) >= numtype_priority(t2))
                    return t1;
            } else {
                if (is_numeric(t1) && is_numeric(t2))
                    return numtype_priority(t1) >= numtype_priority(t2) ? t1 : t2;
            }

            if (t1 == t2) {
                if (is_numeric(t1))
                    return t1;
                else if (t1->kind == DSLType || t1->kind == StringType)
                    return t1;
            } else if (is_numeric(t1)) {
                TYPE_ERR(f, ast, "The right side of this operation is type %s, but it needs to be numeric to do operations with %s",
                         type_to_string(t2), type_to_string(t1));
            } else if (is_numeric(t2)) {
                TYPE_ERR(f, ast, "The left side of this operation is type %s, but it needs to be numeric to do operations with %s",
                         type_to_string(t1), type_to_string(t2));
            }
            TYPE_ERR(f, ast, "Math operations are not supported between %s and %s",
                     type_to_string(t1), type_to_string(t2));
        }

        case Less: case LessEqual: case Greater: case GreaterEqual: {
            bl_type_t *lhs_t = get_type(f, bindings, ast->lhs);
            bl_type_t *rhs_t = get_type(f, bindings, ast->rhs);
            if (lhs_t == rhs_t && (lhs_t->kind == StringType || lhs_t->kind == DSLType))
                return Type(BoolType);
            else if (is_numeric(lhs_t) && is_numeric(rhs_t))
                return Type(BoolType);
            else
                TYPE_ERR(f, ast, "Ordered comparison is not supported for %s and %s",
                         type_to_string(lhs_t), type_to_string(rhs_t));
        }

        case Not: {
            bl_type_t *t = get_type(f, bindings, ast->child);
            if (t->kind == BoolType || is_integral(t))
                return t;
            TYPE_ERR(f, ast, "'not' isn't supported for %s", type_to_string(t)); 
        }

        case Equal: case NotEqual: {
            bl_type_t *lhs_t = get_type(f, bindings, ast->lhs);
            bl_type_t *rhs_t = get_type(f, bindings, ast->rhs);
            if (type_is_a(lhs_t, rhs_t) || type_is_a(rhs_t, lhs_t))
                return Type(BoolType);
            else if (is_numeric(lhs_t) && is_numeric(rhs_t))
                return Type(BoolType);
            else
                TYPE_ERR(f, ast, "These two values have incompatible types: %s vs %s",
                         type_to_string(lhs_t), type_to_string(rhs_t));
        }

        case FunctionDef: case Lambda: {
            NEW_LIST(bl_type_t*, args);
            for (int64_t i = 0; i < LIST_LEN(ast->fn.arg_types); i++) {
                ast_t *arg_def = LIST_ITEM(ast->fn.arg_types, i);
                bl_type_t *t = parse_type(f, bindings, arg_def);
                APPEND(args, t);
            }

            bl_type_t *ret = NULL;
            if (ast->kind == FunctionDef) {
                if (ast->fn.ret_type) {
                    ret = parse_type(f, bindings, ast->fn.ret_type);
                }
            } else {
                hashmap_t *body_bindings = hashmap_new();
                // TODO: strip closure bindings, but allow globals
                // body_bindings->fallback = bindings;
                for (int64_t i = 0; i < LIST_LEN(ast->fn.arg_types); i++) {
                    hashmap_set(body_bindings, LIST_ITEM(ast->fn.arg_names, i), new(binding_t, .type=LIST_ITEM(args, i)));
                }
                ret = get_type(f, body_bindings, ast->fn.body->child);
            }
            if (ret == NULL) ret = Type(VoidType);
            return Type(FunctionType, .args=args, .ret=ret);
        }

        case Struct: {
            if (ast->struct_.name) {
                binding_t *binding = hashmap_get(bindings, ast->struct_.name);
                assert(binding);
                return binding->type->type;
            }
            assert(false);
            // TODO: anonymous structs

//             istr_t name = ast->struct_.name;
//             NEW_LIST(istr_t, field_names);
//             NEW_LIST(bl_type_t*, field_types);
//             LIST_FOR (ast->struct_.members, member, _) {
//                 if ((*member)->kind == StructField) {
//                     APPEND(field_names, (*member)->named.name);
//                     APPEND(field_types, get_type(f, bindings, (*member)->named.value));
//                 } else {
//                     assert(false);
//                 }
//             }
//             return Type(StructType, .struct_.name=name, .struct_.field_names=field_names, .struct_.field_types=field_types);
        }

        case StructDef: {
            istr_t name = ast->struct_.name;
            NEW_LIST(istr_t, field_names);
            NEW_LIST(bl_type_t*, field_types);
            NEW_LIST(istr_t, method_names);
            NEW_LIST(bl_type_t*, method_types);
            bl_type_t *t = Type(StructType, .struct_.name=name, .struct_.field_names=field_names, .struct_.field_types=field_types,
                                .struct_.method_names=method_names, .struct_.method_types=method_types);
            hashmap_t *rec_bindings = hashmap_new();
            rec_bindings->fallback = bindings;
            binding_t b = {.type=Type(TypeType, .type=t)};
            hashmap_set(rec_bindings, name, &b);
            LIST_FOR (ast->struct_.members, member, _) {
                if ((*member)->kind == StructFieldDef) {
                    bl_type_t *ft = parse_type(f, rec_bindings, (*member)->fields.type);
                    LIST_FOR((*member)->fields.names, fname, __) {
                        APPEND(field_names, *fname);
                        APPEND(field_types, ft);
                    }
                } else if ((*member)->kind == FunctionDef) {
                    bl_type_t *mt = get_type(f, rec_bindings, *member);
                    APPEND(method_names, (*member)->fn.name);
                    APPEND(method_types, mt);
                } else {
                    TYPE_ERR(f, *member, "Not yet implemented");
                }
            }
            return Type(TypeType, .type=t);
        }

        case If: {
            bl_type_t *t = NULL;
            LIST_FOR (ast->clauses, clause, _) {
                bl_type_t *clause_t = get_clause_type(f, bindings, clause->condition, clause->body);
                t = type_or_type(t, clause_t);
                if (!t)
                    TYPE_ERR(f, clause->body,
                             "This block has a type %s, which is incompatible with earlier blocks of type %s",
                             type_to_string(clause_t), type_to_string(t));
            }
            if (ast->else_body) {
                bl_type_t *else_type = get_type(f, bindings, ast->else_body);
                t = type_or_type(t, else_type);
                if (!t)
                    TYPE_ERR(f, ast->else_body,
                             "This block has a type %s, which is incompatible with earlier blocks of type %s",
                             type_to_string(else_type), type_to_string(t));
            } else {
                if (t->kind != OptionalType)
                    t = Type(OptionalType, .nonnil=t);
            }
            return t;
        }

        case While: case Repeat: case For: {
            return Type(VoidType);
        }

        default: break;
    }
    TYPE_ERR(f, ast, "Couldn't figure out type for %s", get_ast_kind_name(ast->kind));
}

void check_discardable(file_t *f, hashmap_t *bindings, ast_t *ast)
{
    switch (ast->kind) {
    case AddUpdate: case SubtractUpdate: case DivideUpdate: case MultiplyUpdate:
    case Assign: case Declare: case Block: case FunctionDef: case StructDef:
        return;
    default: {
        bl_type_t *t = get_type(f, bindings, ast);
        if (t->kind == OptionalType)
            t = t->nonnil;

        if (!(t->kind == VoidType || t->kind == AbortType)) {
            TYPE_ERR(f, ast, "This value has a return type of %s but the value is being ignored",
                     type_to_string(t));
        }
    }
    }
}

// vim: ts=4 sw=0 et cino=L2,l1,(0,W4,m1,\:0
