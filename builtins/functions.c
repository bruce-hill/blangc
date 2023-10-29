#include <gc.h>
#include <gc/cord.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/param.h>

#include "../SipHash/halfsiphash.h"
#include "../files.h"
#include "../span.h"
#include "../libsss/hashmap.h"
#include "functions.h"
#include "string.h"

void say(string_t str, string_t end)
{
    if (str.stride == 1) {
        write(STDOUT_FILENO, str.data, str.length);
    } else {
        for (unsigned long i = 0; i < str.length; i++)
            write(STDOUT_FILENO, str.data + i*str.stride, 1);
    }

    if (end.stride == 1) {
        write(STDOUT_FILENO, end.data, end.length);
    } else {
        for (unsigned long i = 0; i < end.length; i++)
            write(STDOUT_FILENO, end.data + i*end.stride, 1);
    }
}

void warn(string_t str, string_t end, bool colorize)
{
    if (colorize) write(STDERR_FILENO, "\x1b[33m", 5);
    if (str.stride == 1) {
        write(STDERR_FILENO, str.data, str.length);
    } else {
        for (unsigned long i = 0; i < str.length; i++)
            write(STDERR_FILENO, str.data + i*str.stride, 1);
    }

    if (end.stride == 1) {
        write(STDERR_FILENO, end.data, end.length);
    } else {
        for (unsigned long i = 0; i < end.length; i++)
            write(STDERR_FILENO, end.data + i*end.stride, 1);
    }
    if (colorize) write(STDERR_FILENO, "\x1b[m", 3);
}

void fail(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    raise(SIGABRT);
}

void fail_array(string_t fmt, ...)
{
    char buf[fmt.length+1];
    for (unsigned long i = 0; i < fmt.length; i++)
        buf[i] = fmt.data[i*fmt.stride];
    buf[fmt.length] = '\0';

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, buf, args);
    va_end(args);
    raise(SIGABRT);
}

string_t last_err()
{
    const char *str = strerror(errno);
    char *copy = GC_MALLOC_ATOMIC(strlen(str)+1);
    strcpy(copy, str);
    return (string_t){.data=copy, .length=strlen(str), .stride=1};
}

static inline char *without_colors(const char *str)
{
    // Strip out color escape sequences: "\x1b[" ... "m"
    size_t fmt_len = strlen(str);
    char *buf = GC_malloc_atomic(fmt_len+1);
    char *dest = buf;
    for (const char *src = str; *src; ++src) {
        if (src[0] == '\x1b' && src[1] == '[') {
            src += 2;
            while (*src && *src != 'm')
                ++src;
        } else {
            *(dest++) = *src;
        }
    }
    *dest = '\0';
    return buf;
}

void sss_doctest(const char *label, CORD expr, const char *type, bool use_color, const char *expected, const char *filename, int start, int end)
{
    static sss_file_t *file = NULL;
    if (filename && (file == NULL || strcmp(file->filename, filename) != 0))
        file = sss_load_file(filename);

    if (filename && file)
        CORD_fprintf(stderr, use_color ? "\x1b[33;1m>>> \x1b[0m%.*s\x1b[m\n" : ">>> %.*s\n", (end - start), file->text + start);

    if (expr) {
        const char *expr_str = CORD_to_const_char_star(expr);
        if (!use_color)
            expr_str = without_colors(expr_str);

        CORD_fprintf(stderr, use_color ? "\x1b[2m%s\x1b[0m %s \x1b[2m: %s\x1b[m\n" : "%s %s : %s\n", label, expr_str, type);
        if (expected) {
            const char *actual = use_color ? without_colors(expr_str) : expr_str;
            if (strcmp(actual, expected) != 0) {
                if (filename && file)
                    fprint_span(stderr, file, file->text+start, file->text+end, "\x1b[31;1m", 2, use_color);
                fail(use_color ? "\x1b[31;1mExpected: \x1b[32;7m%s\x1b[0m\n\x1b[31;1m But got: \x1b[31;7m%s\x1b[0m\n" : "Expected: %s\n But got: %s\n", expected, actual);
            }
        }
    }
}

// vim: ts=4 sw=0 et cino=L2,l1,(0,W4,m1,\:0
