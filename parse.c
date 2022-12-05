// Run a BP parser to get a match and convert that to an AST structure

#include <bp/files.h>
#include <bp/match.h>
#include <bp/pattern.h>
#include <bp/printmatch.h>
#include <bp/json.h>
#include <ctype.h>
#include <err.h>
#include <gc.h>
#include <gc/cord.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "libblang/list.h"
#include "parse.h"
#include "util.h"

#ifndef streq
#define streq(a,b) (strcmp(a,b) == 0)
#endif

#ifndef strneq
#define strneq(a,b,n) (strncmp(a,b,n) == 0)
#endif

#define AST(m, mykind, ...) (new(ast_t, .kind=mykind, .match=m __VA_OPT__(,) __VA_ARGS__))

static pat_t *grammar = NULL;
static file_t *loaded_files = NULL;

file_t *parsing = NULL;

//
// If there was a parse error while building a pattern, print an error message and exit.
//
static inline pat_t *assert_pat(file_t *f, maybe_pat_t maybe_pat)
{
    if (!maybe_pat.success) {
        const char *err_start = maybe_pat.value.error.start,
              *err_end = maybe_pat.value.error.end,
              *err_msg = maybe_pat.value.error.msg;

        const char *nl = memrchr(f->start, '\n', (size_t)(err_start - f->start));
        const char *sol = nl ? nl+1 : f->start;
        nl = memchr(err_start, '\n', (size_t)(f->end - err_start));
        const char *eol = nl ? nl : f->end;
        if (eol < err_end) err_end = eol;

        fprintf(stderr, "\033[31;7;1m%s\033[0m\n", err_msg);
        fprintf(stderr, "%.*s\033[41;30m%.*s\033[m%.*s\n",
                (int)(err_start - sol), sol,
                (int)(err_end - err_start), err_start,
                (int)(eol - err_end), err_end);
        fprintf(stderr, "\033[34;1m");
        const char *p = sol;
        for (; p < err_start; ++p) (void)fputc(*p == '\t' ? '\t' : ' ', stderr);
        if (err_start == err_end) ++err_end;
        for (; p < err_end; ++p)
            if (*p == '\t')
                // Some janky hacks: 8 ^'s, backtrack 8 spaces, move forward a tab stop, clear any ^'s that overshot
                fprintf(stderr, "^^^^^^^^\033[8D\033[I\033[K");
            else
                (void)fputc('^', stderr);
        fprintf(stderr, "\033[m\n");
        exit(EXIT_FAILURE);
    }
    return maybe_pat.value.pat;
}

//
// Initialize the Blang syntax pattern
//
static void load_grammar(void)
{
    file_t *builtins_file = load_file(&loaded_files, "/etc/bp/builtins.bp");
    file_t *blang_syntax = load_file(&loaded_files, "syntax.bp");
    grammar = chain_together(
        assert_pat(builtins_file, bp_pattern(builtins_file->start, builtins_file->end)),
        assert_pat(blang_syntax, bp_pattern(blang_syntax->start, blang_syntax->end)));
}

//
// Print error information from a match
//
static void print_err(file_t *f, match_t *m, int context) {
    fprintf(stderr, "\x1b[31;7;1m Syntax Error: \x1b[0;31;1m ");
    fprint_match(stderr, f->start, m, NULL);
    fprintf(stderr, "\x1b[m\n\n");
    highlight_match(stderr, f, m, context);
}

//
// Report any errors and exit
//
static void report_errors(file_t *f, match_t *m, bool stop_on_first)
{
    pat_t *pat = m->pat;
    if (pat->type == BP_TAGGED && strncmp(pat->args.capture.name, "ParseError", pat->args.capture.namelen) == 0) {
        print_err(f, m, 2);
        if (stop_on_first)
            exit(1);
    }
    if (m->children) {
        for (int i = 0; m->children[i]; i++)
            report_errors(f, m->children[i], stop_on_first);
    }
}

const char *kind_tags[] = {
    [Unknown] = "???", [Nil]="Nil", [Bool]="Bool", [Var]="Var",
    [Int]="Int", [Num]="Num", [Range]="Range",
    [StringLiteral]=NULL, [StringJoin]="String", [DSL]="DSL", [Interp]="Interp",
    [Declare]="Declaration", [Assign]="Assignment",
    [AddUpdate]="AddUpdate", [SubtractUpdate]="SubUpdate", [MultiplyUpdate]="MulUpdate", [DivideUpdate]="DivUpdate",
    [AndUpdate]="AndUpdate", [OrUpdate]="OrUpdate",
    [Add]="Add", [Subtract]="Subtract", [Multiply]="Multiply", [Divide]="Divide", [Power]="Power", [Modulus]="Mod",
    [And]="And", [Or]="Or", [Xor]="Xor",
    [Equal]="Equal", [NotEqual]="NotEqual", [Greater]="Greater", [GreaterEqual]="GreaterEq", [Less]="Less", [LessEqual]="LessEq",
    [Not]="Not", [Negative]="Negative", [Len]="Len", [Maybe]="Maybe",
    [TypeOf]="TypeOf", [SizeOf]="SizeOf",
    [List]="List", [Table]="Table",
    [FunctionDef]="FnDef", [MethodDef]="MethodDef", [Lambda]="Lambda",
    [FunctionCall]="FnCall", [KeywordArg]="KeywordArg",
    [Block]="Block",
    [Do]="Do", [If]="If", [For]="For", [While]="While", [Repeat]="Repeat", [When]="When",
    [Skip]="Skip", [Stop]="Stop",
    [Return]="Return",
    [Fail]="Fail",
    [TypeName]="TypeVar",
    [TypeList]="ListType", [TypeTable]="TableType",
    [TypeFunction]="FnType", [TypeOption]="OptionalType",
    [Cast]="Cast", [As]="As", [Extern]="Extern",
    [Struct]="Struct", [StructDef]="StructDef", [StructField]="StructField", [StructFieldDef]="StructFieldDef",
    [EnumDef]="EnumDef",
    [Index]="IndexedTerm", [FieldName]="FieldName",
};

static astkind_e get_kind(match_t *m)
{
    const char *tag = m->pat->args.capture.name;
    size_t len = m->pat->args.capture.namelen;
    for (size_t i = 0; i < sizeof(kind_tags)/sizeof(kind_tags[0]); i++) {
        if (!kind_tags[i]) continue;
        if (strncmp(kind_tags[i], tag, len) == 0 && kind_tags[i][len] == '\0')
            return (astkind_e)i;
    }
    if (strncmp(tag, "AddSub", len) == 0) {
        match_t *op = get_named_capture(m, "op", -1);
        return *op->start == '+' ? Add : Subtract;
    } else if (strncmp(tag, "MulDiv", len) == 0) {
        match_t *op = get_named_capture(m, "op", -1);
        return *op->start == '*' ? Multiply : Divide;
    }
    return Unknown;
}


//
// Convert a match to an interned string
//
static istr_t match_to_istr(match_t *m)
{
    if (!m) return NULL;
    // Rough estimate of size
    FILE *f = fmemopen(NULL, 2*(size_t)(m->end - m->start) + 1, "r+");
    fprint_match(f, m->start, m, NULL);
    fputc('\0', f);
    fseek(f, 0, SEEK_SET);
    CORD c = CORD_from_file_eager(f);
    return intern_str(CORD_to_const_char_star(c));
}

//
// Convert a match structure (from BP) into an AST structure (for Blang)
//
ast_t *match_to_ast(match_t *m)
{
    if (!m) return NULL;
    pat_t *pat = m->pat;
    if (pat->type == BP_TAGGED) {
        astkind_e kind = get_kind(m);
        switch (kind) {
        case Nil: {
            ast_t *type = match_to_ast(get_named_capture(m, "type", -1));
            return AST(m, kind, .child=type);
        }
        case Bool: return AST(m, Bool, .b=strncmp(m->start, "no", 2) != 0);
        case Var: {
            istr_t v = intern_strn(m->start, (size_t)(m->end - m->start));
            return AST(m, Var, .str=v);
        }
        case Int: {
            int64_t i;
            char buf[(int)(m->end - m->start + 1)];
            char *dest = buf;
            const char *start = m->start;
            bool negative = *start == '-';
            if (*start == '-')
                ++start;
            else if (*start == '+')
                ++start;
            for (const char *src = start; src < m->end; ++src)
                if (isalnum(*src))
                    *(dest++) = *src;
            *dest = '\0';
            if (strncmp(buf, "0x", 2) == 0)
                i = strtol(buf+2, NULL, 16);
            else if (strncmp(buf, "0o", 2) == 0)
                i = strtol(buf+2, NULL, 8);
            else if (strncmp(buf, "0b", 2) == 0)
                i = strtol(buf+2, NULL, 2);
            else
                i = strtol(buf, NULL, 10);

            return AST(m, Int, .i=negative ? -i : i);
        }
        case Num: {
            double n = strtod(m->start, NULL);
            return AST(m, Num, .n=n);
        }
        case Range: {
            return AST(m, Range,
                       .range.first=match_to_ast(get_named_capture(m, "first", -1)),
                       .range.last=match_to_ast(get_named_capture(m, "last", -1)),
                       .range.step=match_to_ast(get_named_capture(m, "step", -1)));
        }
        case StringJoin: {
            match_t *content = get_named_capture(m, "content", -1);
            List(ast_t*) chunks = EMPTY_LIST(ast_t*);
            const char *prev = content->start;
            for (int i = 1; ; i++) { // index 1 == text content
                match_t *cm = get_numbered_capture(content, i);
                assert(cm != content);
                if (!cm) break;
                if (cm->start > prev) {
                    APPEND(chunks, AST(m, StringLiteral, .str=intern_strn(prev, (size_t)(cm->start - prev))));
                }
                assert(match_to_ast(cm));
                APPEND(chunks, match_to_ast(cm));
                prev = cm->end;
            }
            if (content->end > prev) {
                APPEND(chunks, AST(m, StringLiteral, .str=intern_strn(prev, (size_t)(content->end - prev))));
            }
            return AST(m, StringJoin, .children=chunks);
        }
        case Interp: {
            return match_to_ast(get_named_capture(m, "value", -1));
        }
        case List: {
            match_t *type_m = get_named_capture(m, "type", -1);
            if (type_m)
                return AST(m, List, .list.type=match_to_ast(type_m));
            
            List(ast_t*) items = EMPTY_LIST(ast_t*);
            for (int i = 1; ; i++) {
                match_t *im = get_numbered_capture(m, i);
                assert(im != m);
                if (!im) break;
                // Shim: [x if cond] is mapped to [x if cond else skip]
                // `skip` in a list means "don't add this item"
                ast_t *item = match_to_ast(im);
                if (item->kind == If && !item->else_body)
                    item->else_body = AST(item->match, Skip);
                APPEND(items, item);
            }
            return AST(m, List, .list.items=items);
        }
        case Do: case Block: {
            NEW_LIST(ast_t*, children);
            for (int i = 1; ; i++) {
                ast_t *child = match_to_ast(get_numbered_capture(m, i));
                if (!child) break;
                APPEND(children, child);
            }
            return AST(m, kind, .children=children);
        }
        case FunctionDef: case MethodDef: case Lambda: {
            istr_t name = kind == Lambda ? NULL : match_to_istr(get_named_capture(m, "name", -1));
            NEW_LIST(istr_t, arg_names);
            NEW_LIST(ast_t*, arg_types);
            match_t *args_m = get_named_capture(m, "args", -1);
            for (int i = 1; ; i++) {
                match_t *arg_m = get_numbered_capture(args_m, i);
                if (!arg_m) break;
                match_t *arg_name = get_named_capture(arg_m, "name", -1);
                match_t *arg_type = get_named_capture(arg_m, "type", -1);
                assert(arg_name != NULL && arg_type != NULL);
                APPEND(arg_names, match_to_istr(arg_name));
                APPEND(arg_types, match_to_ast(arg_type));
            }
            match_t *ret_m = get_named_capture(m, "returnType", -1);
            ast_t *ret_type = ret_m ? match_to_ast(ret_m) : NULL;
            match_t *body_m = get_named_capture(m, "body", -1);
            ast_t *body = match_to_ast(body_m);

            if (kind == Lambda)
                body = AST(body_m, Return, .child=body);

            istr_t self = NULL;
            if (kind == MethodDef)
                self = match_to_istr(get_named_capture(m, "selfVar", -1));

            return AST(m, kind, .fn.name=name, .fn.self=self,
                       .fn.arg_names=arg_names, .fn.arg_types=arg_types,
                       .fn.ret_type=ret_type, .fn.body=body);
        }
        case FunctionCall: {
            match_t *fn_m = get_named_capture(m, "fn", -1);
            ast_t *fn = match_to_ast(fn_m);
            NEW_LIST(ast_t*, args);
            for (int i = 1; ; i++) {
                ast_t *arg = match_to_ast(get_numbered_capture(m, i));
                if (!arg) break;
                APPEND(args, arg);
            }
            return AST(m, FunctionCall, .call.fn=fn, .call.args=args);
        }
        case KeywordArg: case StructField: {
            istr_t name = match_to_istr(get_named_capture(m, "name", -1));
            ast_t *value = match_to_ast(get_named_capture(m, "value", -1));
            return AST(m, kind, .named.name=name, .named.value=value);
        }
        case Return: {
            return AST(m, Return, .child=match_to_ast(get_named_capture(m, "value", -1)));
        }
        case StructDef: case Struct: {
            istr_t name = match_to_istr(get_named_capture(m, "name", -1));
            NEW_LIST(ast_t*, members);
            for (int i = 1; ; i++) {
                ast_t *member = match_to_ast(get_numbered_capture(m, i));
                if (!member) break;
                APPEND(members, member);
            }
            return AST(m, kind, .struct_.name=name, .struct_.members=members);
        }
        case StructFieldDef: {
            ast_t *type = match_to_ast(get_named_capture(m, "type", -1));
            NEW_LIST(istr_t, names);
            match_t *names_m = get_named_capture(m, "names", -1);
            for (int i = 1; ; i++) {
                istr_t name = match_to_istr(get_numbered_capture(names_m, i));
                if (!name) break;
                APPEND(names, name);
            }
            return AST(m, StructFieldDef, .fields.names=names, .fields.type=type);
        }
        case EnumDef: {
            istr_t name = match_to_istr(get_named_capture(m, "name", -1));
            NEW_LIST(istr_t, field_names);
            NEW_LIST(int64_t, field_values);
            int64_t next_value = 0;
            for (int i = 1; ; i++) {
                match_t *field_m = get_numbered_capture(m, i);
                if (!field_m) break;
                istr_t name = match_to_istr(get_named_capture(field_m, "name", -1));
                ast_t *value = match_to_ast(get_named_capture(field_m, "value", -1));
                if (value)
                    next_value = value->i;
                APPEND(field_names, name);
                APPEND(field_values, next_value);
                // // Workaround because this is an array-of-structs instead of pointers:
                // // (Can't use APPEND() macro)
                // list_append((list_t*)fields, sizeof(enum_field_t), &field);
                ++next_value;
            }
            return AST(m, EnumDef, .enum_.name=name, .enum_.field_names=field_names, .enum_.field_values=field_values);
        }
        case FieldName: {
            return AST(m, FieldName, .str=match_to_istr(m));
        }
        case Index: {
            ast_t *indexed = match_to_ast(get_named_capture(m, "value", -1));
            ast_t *index = match_to_ast(get_named_capture(m, "index", -1));
            if (index->kind == FieldName)
                return AST(m, FieldAccess, .fielded=indexed, .field=index->str);
            return AST(m, Index, .indexed=indexed, .index=index);
        }
        case If: {
            NEW_LIST(ast_clause_t, clauses);
            for (int i = 1; ; i++) {
                match_t *clause_m = get_numbered_capture(m, i);
                if (!clause_m) break;
                match_t *condition_m = get_named_capture(clause_m, "condition", -1);
                match_t *body_m = get_named_capture(clause_m, "body", -1);
                assert(condition_m && body_m);
                ast_clause_t clause = {
                    .condition=match_to_ast(condition_m),
                    .body=match_to_ast(body_m),
                };
                // Workaround because this is an array-of-structs instead of pointers:
                // (Can't use APPEND() macro)
                list_append((list_t*)clauses, sizeof(ast_clause_t), &clause);
            }
            ast_t *else_block = match_to_ast(get_named_capture(m, "elseBody", -1));
            return AST(m, If, .clauses=clauses, .else_body=else_block);
        }
        case When: {
            ast_t *subject = match_to_ast(get_named_capture(m, "subject", -1));
            NEW_LIST(ast_cases_t, cases);
            for (int i = 1; ; i++) {
                match_t *clause_m = get_numbered_capture(m, i);
                if (!clause_m) break;
                match_t *cases_m = get_named_capture(clause_m, "cases", -1);
                NEW_LIST(ast_t*, values);
                for (int casenum = 1; ; casenum++) {
                    ast_t *caseval = match_to_ast(get_numbered_capture(cases_m, casenum));
                    if (!caseval) break;
                    APPEND(values, caseval);
                }
                ast_t *casebody = match_to_ast(get_named_capture(clause_m, "body", -1));
                ast_cases_t case_ = {
                    .cases=values,
                    .body=casebody,
                };
                // Workaround because this is an array-of-structs instead of pointers:
                // (Can't use APPEND() macro)
                list_append((list_t*)cases, sizeof(ast_cases_t), &case_);
            }
            ast_t *else_block = match_to_ast(get_named_capture(m, "elseBody", -1));
            return AST(m, When, .subject=subject, .cases=cases, .default_body=else_block);
        }
        case While: case Repeat: {
            ast_t *condition = match_to_ast(get_named_capture(m, "condition", -1));
            ast_t *body = match_to_ast(get_named_capture(m, "body", -1));
            ast_t *filter = match_to_ast(get_named_capture(m, "filter", -1));
            if (filter)
                body = AST(m, Block, .children=LIST(ast_t*, filter, body));
            ast_t *between = match_to_ast(get_named_capture(m, "between", -1));
            return AST(m, kind, .loop.condition=condition, .loop.body=body, .loop.between=between);
        }
        case For: {
            ast_t *iter = match_to_ast(get_named_capture(m, "iterable", -1));
            ast_t *key = match_to_ast(get_named_capture(m, "index", -1));
            ast_t *value = match_to_ast(get_named_capture(m, "val", -1));
            ast_t *body = match_to_ast(get_named_capture(m, "body", -1));
            ast_t *filter = match_to_ast(get_named_capture(m, "filter", -1));
            if (filter)
                body = AST(m, Block, .children=LIST(ast_t*, filter, body));
            ast_t *between = match_to_ast(get_named_capture(m, "between", -1));
            return AST(m, For, .for_loop.iter=iter, .for_loop.key=key, .for_loop.value=value,
                       .for_loop.body=body, .for_loop.between=between);
        }
        case Skip: case Stop: {
            istr_t target = match_to_istr(get_named_capture(m, "target", -1));
            return AST(m, kind, .str=target);
        }
        case Add: case Subtract: case Multiply: case Divide: case Power: case Modulus:
        case AddUpdate: case SubtractUpdate: case MultiplyUpdate: case DivideUpdate:
        case And: case Or: case Xor:
        case Equal: case NotEqual: case Less: case LessEqual: case Greater: case GreaterEqual:
        case Declare: {
            ast_t *lhs = match_to_ast(get_named_capture(m, "lhs", -1));
            ast_t *rhs = match_to_ast(get_named_capture(m, "rhs", -1));
            return AST(m, kind, .lhs=lhs, .rhs=rhs);
        }
        case Cast: case As: {
            ast_t *expr = match_to_ast(get_named_capture(m, "expr", -1));
            ast_t *type = match_to_ast(get_named_capture(m, "type", -1));
            return AST(m, kind, .expr=expr, .type=type);
        }
        case Extern: {
            ast_t *expr = match_to_ast(get_named_capture(m, "name", -1));
            ast_t *type = match_to_ast(get_named_capture(m, "type", -1));
            return AST(m, kind, .expr=expr, .type=type);
        }
        case Not: case Negative: case Len: case Maybe: case TypeOf: case SizeOf: {
            ast_t *child = match_to_ast(get_named_capture(m, "value", -1));
            return AST(m, kind, .child=child);
        }
        case Assign: {
            NEW_LIST(ast_t*, lhs);
            NEW_LIST(ast_t*, rhs);
            match_t *lhses = get_named_capture(m, "lhs", -1);
            match_t *rhses = get_named_capture(m, "rhs", -1);
            for (int64_t i = 1; ; i++) {
                ast_t *var = match_to_ast(get_numbered_capture(get_numbered_capture(lhses, 1), i));
                if (var && var->kind != Var) {
                    fprintf(stderr, "\x1b[31;7;1mOnly variables can be assigned to\x1b[m\n\n");
                    highlight_match(stderr, parsing, var->match, 2);
                    exit(1);
                }
                ast_t *val = match_to_ast(get_numbered_capture(get_numbered_capture(rhses, 1), i));
                if (!var && !val) {
                    break;
                } else if (var && !val) {
                    fprintf(stderr, "\x1b[31;7;1mThis term is missing a value to assign it\x1b[m\n\n");
                    highlight_match(stderr, parsing, var->match, 2);
                    exit(1);
                } else if (val && !var) {
                    fprintf(stderr, "\x1b[31;7;1mThis value doesn't have a corresponding term to assign to\x1b[m\n\n");
                    highlight_match(stderr, parsing, val->match, 2);
                    exit(1);
                }

                APPEND(lhs, var);
                APPEND(rhs, val);
            }
            return AST(m, kind, .multiassign.lhs=lhs, .multiassign.rhs=rhs);
        }
        case Fail: {
            ast_t *msg = match_to_ast(get_named_capture(m, "message", -1));
            return AST(m, Fail, .child=msg);
        }
        case TypeOption: {
            ast_t *nonnil = match_to_ast(get_named_capture(m, "nonnil", -1));
            return AST(m, TypeOption, .child=nonnil);
        }
        case TypeName: {
            istr_t name = match_to_istr(m);
            return AST(m, TypeName, .str=name);
        }
        case TypeList: {
            ast_t *item_t = match_to_ast(get_named_capture(m, "itemType", -1));
            return AST(m, TypeList, .child=item_t);
        }
        case TypeFunction: {
            ast_t *ret = match_to_ast(get_named_capture(m, "returnType", -1));
            assert(ret);
            match_t *args_m = get_named_capture(m, "args", -1);
            NEW_LIST(ast_t*, arg_types);
            NEW_LIST(istr_t, arg_names);
            for (int64_t i = 1; ; i++) {
                match_t *arg_m = get_numbered_capture(args_m, i);
                if (!arg_m) break;
                istr_t arg_name = match_to_istr(get_named_capture(arg_m, "name", -1));
                ast_t *arg_t = match_to_ast(get_named_capture(arg_m, "type", -1));
                APPEND(arg_names, arg_name);
                APPEND(arg_types, arg_t);
            }
            return AST(m, TypeFunction, .fn.ret_type=ret, .fn.arg_names=arg_names, .fn.arg_types=arg_types);
        }
        default: break;
        }

        const char *tag = m->pat->args.capture.name;
        size_t taglen = m->pat->args.capture.namelen;
        if (strneq(tag, "Newline", taglen)) {
            return AST(m, StringLiteral, .str=intern_str("\n"));
        } else if (strneq(tag, "Escape", taglen)) {
            static const char *unescapes[255] = {['a']="\a",['b']="\b",['e']="\e",['f']="\f",['n']="\n",['r']="\r",['t']="\t",['v']="\v"};
            if (unescapes[(int)m->start[1]]) {
                return AST(m, StringLiteral, .str=intern_str(unescapes[(int)m->start[1]]));
            } else if (m->start[1] == 'x') {
                char *endptr = (char*)(m->start + 4);
                CORD c = CORD_cat_char(NULL, (char)strtol(m->start+2, &endptr, 16));
                return AST(m, StringLiteral, .str=intern_str(CORD_to_char_star(c)));
            } else if ('0' <= m->start[1] && m->start[1] <= '7') {
                char *endptr = (char*)(m->start + 4);
                CORD c = CORD_cat_char(NULL, (char)strtol(m->start+2, &endptr, 8));
                return AST(m, StringLiteral, .str=intern_str(CORD_to_char_star(c)));
            } else {
                return AST(m, StringLiteral, .str=intern_strn(m->start+1, 1));
            }
            return AST(m, StringLiteral, .str=match_to_istr(m));
        } else {
            fprintf(stderr, "\x1b[31;7;1mParsing isn't fully implemented for AST tag: %.*s\x1b[m\n\n", (int)pat->args.capture.namelen, pat->args.capture.name);
            highlight_match(stderr, parsing, m, 2);
            exit(1);
        }
    } else if (m->children) {
        for (int i = 0; m->children[i]; i++) {
            ast_t *ast = match_to_ast(m->children[i]);
            if (ast) return ast;
        }
    }
    return NULL;
}

//
// Print an AST (for debugging)
//
void print_ast(ast_t *ast) {
    if (!ast) {
        printf("\x1b[31;1m(NULL)\x1b[m");
        return;
    }
    switch (ast->kind) {
    case Bool: printf("\x1b[35m%s\x1b[m", ast->b ? "yes" : "no"); break;
    case Int: printf("\x1b[35m%ld\x1b[m", ast->i); break;
    case Num: printf("\x1b[35m%g\x1b[m", ast->n); break;
    case Var: printf("\x1b[1m%s\x1b[m", ast->str); break;
    case FunctionCall: {
        print_ast(ast->call.fn);
        printf("(");
        for (int64_t i = 0; i < LIST_LEN(ast->call.args); i++) {
            if (i > 0)
                printf(", ");
            print_ast(LIST_ITEM(ast->call.args, i));
        }
        printf(")");
        break;
    }
    case KeywordArg: {
        printf("\x1b[0;2m%s=\x1b[m", ast->named.name);
        print_ast(ast->named.value);
        break;
    }
    case StringJoin: {
        for (int64_t i = 0; i < LIST_LEN(ast->children); i++) {
            if (i > 0) printf("..");
            print_ast(LIST_ITEM(ast->children, i));
        }
        break;
    }
    case StringLiteral: printf("\x1b[35m\"%s\"\x1b[m", ast->str); break;
    case Block: {
        for (int64_t i = 0; i < LIST_LEN(ast->children); i++) {
            printf("\x1b[2m%ld |\x1b[m ", i+1);
            print_ast(LIST_ITEM(ast->children, i));
            printf("\n");
        }
        break;
    }

    case Add: case Subtract: case Multiply: case Divide: case Power: case Modulus:
    case And: case Or: case Xor:
    case Equal: case NotEqual: case Less: case LessEqual: case Greater: case GreaterEqual:
    case Declare: case Cast: case As: {
        printf("%s(", get_ast_kind_name(ast->kind));
        print_ast(ast->lhs);
        printf(",");
        print_ast(ast->rhs);
        printf(")");
        break;
    }

    case While: {
        printf("While(");
        print_ast(ast->loop.condition);
        printf(",");
        print_ast(ast->loop.body);
        printf(")");
        break;
    }

    case Fail: {
        printf("\x1b[33mfail\x1b[m ");
        print_ast(ast->child);
        break;
    }
    default: printf("%s(...)", kind_tags[ast->kind]);
    }
}

ast_t *parse(file_t *f)
{
    if (grammar == NULL) load_grammar();
    parsing = f;
    match_t *m = NULL;
    ast_t *ast = NULL;
    if (next_match(&m, f->start, f->end, grammar, grammar, NULL, false)) {
        if (m->start > f->start) {
            fprintf(stderr, "File contains junk at the front\n");
            exit(1);
        } else if (m->end < f->end) {
            fprintf(stderr, "File contains junk at the end\n");
            exit(1);
        } else {
            report_errors(f, m, true);
            ast = match_to_ast(m);
        }
    }
    parsing = NULL;
    return ast;
}
// vim: ts=4 sw=0 et cino=L2,l1,(0,W4,m1,\:0
