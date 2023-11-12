//
// files.c - Implementation of some file loading functionality.
//

#include <err.h>
#include <fcntl.h>
#include <gc.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "builtins/array.h"
#include "files.h"
#include "util.h"

static const int tabstop = 4;

public char *resolve_path(const char *path, const char *relative_to)
{
    if (!relative_to || streq(relative_to, "/dev/stdin")) relative_to = ".";
    if (!path || strlen(path) == 0) return NULL;

    // Resolve the path to an absolute path, assuming it's relative to the file
    // it was found in:
    char buf[PATH_MAX] = {0};
    if (streq(path, "~") || strncmp(path, "~/", 2) == 0) {
        char *resolved = realpath(heap_strf("%s%s", getenv("HOME"), path+1), buf);
        if (resolved) return heap_str(resolved);
    } else if (streq(path, ".") || strncmp(path, "./", 2) == 0) {
        char *relative_dir = dirname(heap_str(relative_to));
        char *resolved = realpath(heap_strf("%s/%s", relative_dir, path), buf);
        if (resolved) return heap_str(resolved);
    } else if (path[0] == '/') {
        // Absolute path:
        char *resolved = realpath(path, buf);
        if (resolved) return heap_str(resolved);
    } else {
        // Relative path:
        char *blpath = heap_str(getenv("SSSPATH"));
        char *relative_dir = dirname(heap_str(relative_to));
        for (char *dir; (dir = strsep(&blpath, ":")); ) {
            if (dir[0] == '/') {
                char *resolved = realpath(heap_strf("%s/%s", dir, path), buf);
                if (resolved) return heap_str(resolved);
            } else if (dir[0] == '~' && (dir[1] == '\0' || dir[1] == '/')) {
                char *resolved = realpath(heap_strf("%s%s/%s", getenv("HOME"), dir, path), buf);
                if (resolved) return heap_str(resolved);
            } else if (streq(dir, ".") || strncmp(dir, "./", 2) == 0) {
                char *resolved = realpath(heap_strf("%s/%s", relative_dir, path), buf);
                if (resolved) return heap_str(resolved);
            } else if (streq(dir, ".") || streq(dir, "..") || strncmp(dir, "./", 2) == 0 || strncmp(dir, "../", 3) == 0) {
                char *resolved = realpath(heap_strf("%s/%s/%s", relative_dir, dir, path), buf);
                if (resolved) return heap_str(resolved);
            } else {
                char *resolved = realpath(heap_strf("%s/%s", dir, path), buf);
                if (resolved) return heap_str(resolved);
            }
        }
    }
    return NULL;
}

static sss_file_t *_load_file(const char* filename, FILE *file)
{
    if (!file) return NULL;

    size_t file_size = 0, line_cap = 0;
    char *file_buf = NULL, *line_buf = NULL;
    FILE *mem = open_memstream(&file_buf, &file_size);
    ssize_t line_len = 0;
    array_t lines = {.stride=sizeof(sss_line_t), .atomic=1};
    while ((line_len = getline(&line_buf, &line_cap, file)) >= 0) {
        sss_line_t line_info = {.offset=file_size, .indent=0, .is_empty=false};
        char *p;
        for (p = line_buf; *p == ' ' || *p == '\t'; ++p)
            line_info.indent += *p == ' ' ? 1 : 4;
        line_info.is_empty = *p != '\r' && *p != '\n';
        Array_insert(&lines, &line_info, lines.length+1, sizeof(line_info));
        fwrite(line_buf, sizeof(char), line_len, mem);
        fflush(mem);
    }
    fclose(file);

    char *copy = GC_MALLOC_ATOMIC(file_size+1);
    memcpy(copy, file_buf, file_size);
    copy[file_size] = '\0';
    fclose(mem);

    free(file_buf);
    const char *relative_filename = filename;
    if (filename && filename[0] != '<' && !streq(filename, "/dev/stdin")) {
        filename = resolve_path(filename, ".");
        // Convert to relative path (if applicable)
        char buf[PATH_MAX];
        char *cwd = getcwd(buf, sizeof(buf));
        size_t cwd_len = strlen(cwd);
        if (strncmp(cwd, filename, cwd_len) == 0 && filename[cwd_len] == '/')
            relative_filename = &filename[cwd_len+1];
    }
    return new(sss_file_t, .filename=filename, .relative_filename=relative_filename, .text=copy, .len=file_size, .lines=lines);
}

//
// Read an entire file into memory.
//
public sss_file_t *sss_load_file(const char* filename)
{
    FILE *file = filename[0] ? fopen(filename, "r") : stdin;
    return _load_file(filename, file);
}

//
// Create a virtual file from a string.
//
public sss_file_t *sss_spoof_file(const char* filename, const char *text)
{
    FILE *file = fmemopen((char*)text, strlen(text)+1, "r");
    return _load_file(filename, file);
}

//
// Given a pointer, determine which line number it points to (1-indexed)
//
public size_t sss_get_line_number(sss_file_t *f, const char *p)
{
    // Binary search:
    ssize_t lo = 0, hi = (ssize_t)f->lines.length-1;
    if (p < f->text) return 0;
    size_t offset = (ssize_t)(p - f->text);
    while (lo <= hi) {
        ssize_t mid = (lo + hi) / 2;
        sss_line_t *line = ((sss_line_t*)(f->lines.data + mid*f->lines.stride));
        if (line->offset == offset)
            return mid + 1;
        else if (line->offset < offset)
            lo = mid + 1;    
        else if (line->offset > offset)
            hi = mid - 1;
    }
    return (size_t)lo; // Return the line number whose line starts closest before p
}

//
// Given a pointer, determine which line column it points to.
//
public size_t sss_get_line_column(sss_file_t *f, const char *p)
{
    size_t line_no = sss_get_line_number(f, p);
    sss_line_t *line = ((sss_line_t*)(f->lines.data + (line_no-1)*f->lines.stride));
    return 1 + (size_t)(p - (f->text + line->offset));
}

//
// Given a pointer, get the indentation of the line it's on.
//
public size_t sss_get_indent(sss_file_t *f, const char *p)
{
    ssize_t line_no = sss_get_line_number(f, p);
    sss_line_t *line = ((sss_line_t*)(f->lines.data + (line_no-1)*f->lines.stride));
    return line->indent;
}

//
// Return a pointer to the line with the specified line number (1-indexed)
//
public const char *sss_get_line(sss_file_t *f, size_t line_number)
{
    if (line_number == 0 || line_number > (size_t)f->lines.length) return NULL;
    sss_line_t *line = ((sss_line_t*)(f->lines.data + (line_number-1)*f->lines.stride));
    return f->text + line->offset;
}

//
// Return a value like /foo:line:col
//
public const char *sss_get_file_pos(sss_file_t *f, const char *p)
{
    return heap_strf("%s:%ld:%ld", f->filename, sss_get_line_number(f, p), sss_get_line_column(f, p));
}

static int fputc_column(FILE *out, char c, char print_char, int *column)
{
    int printed = 0;
    if (print_char == '\t') print_char = ' ';
    if (c == '\t') {
        for (int to_fill = tabstop - (*column % tabstop); to_fill > 0; --to_fill) {
            printed += fputc(print_char, out);
            ++*column;
        }
    } else {
        printed += fputc(print_char, out);
        ++*column;
    }
    return printed;
}

//
// Print a span from a file
//
public int fprint_span(FILE *out, sss_file_t *file, const char *start, const char *end, const char *hl_color, size_t context_lines, bool use_color)
{
    if (!file) return 0;

    // Handle spans that come from multiple files:
    if (start < file->text || start > file->text + file->len)
        start = end;
    if (end < file->text || end > file->text + file->len)
        end = start;
    // Just in case neither end of the span came from this file:
    if (end < file->text || end > file->text + file->len)
        start = end = file->text;

    const char *lineno_fmt, *normal_color, *empty_marker;
    bool print_carets = false;
    int printed = 0;
    if (use_color) {
        lineno_fmt = "\x1b[0;2m%*lu\x1b(0\x78\x1b(B\x1b[m ";
        normal_color = "\x1b[m";
        empty_marker = "\x1b(0\x61\x1b(B";
        printed += fprintf(out, "\x1b[33;4;1m%s\x1b[m\n", file->relative_filename);
    } else {
        lineno_fmt = "%*lu| ";
        hl_color = "";
        normal_color = "";
        empty_marker = " ";
        print_carets = true;
        printed += fprintf(out, "%s\n", file->relative_filename);
    }

    if (context_lines == 0)
        return fprintf(out, "%s%.*s%s", hl_color, (int)(end - start), start, normal_color);

    ssize_t start_line = sss_get_line_number(file, start),
            end_line = sss_get_line_number(file, end);

    ssize_t first_line = start_line - (context_lines - 1),
            last_line = end_line + (context_lines - 1);

    if (first_line < 1) first_line = 1;
    if (last_line > file->lines.length) last_line = file->lines.length;

    int digits = 1;
    for (size_t i = last_line; i > 0; i /= 10) ++digits;

    for (ssize_t line_no = first_line; line_no <= last_line; ++line_no) {
        if (line_no > first_line + 5 && line_no < last_line - 5) {
            if (use_color)
                printed += fprintf(out, "\x1b[0;2;3;4m     ... %ld lines omitted ...     \x1b[m\n", (last_line - first_line) - 11);
            else
                printed += fprintf(out, "     ... %ld lines omitted ...\n", (last_line - first_line) - 11);
            line_no = last_line - 6;
            continue;
        }

        printed += fprintf(out, lineno_fmt, digits, line_no);
        const char *line = sss_get_line(file, line_no);
        if (!line) break;

        int column = 0;
        const char *p = line;
        // Before match
        for (; *p && *p != '\r' && *p != '\n' && p < start; ++p)
            printed += fputc_column(out, *p, *p, &column);

        // Zero-width matches
        if (p == start && start == end) {
            printed += fprintf(out, "%s%s%s", hl_color, empty_marker, normal_color);
            column += 1;
        }

        // Inside match
        if (start <= p && p < end) {
            printed += fputs(hl_color, out);
            for (; *p && *p != '\r' && *p != '\n' && p < end; ++p)
                printed += fputc_column(out, *p, *p, &column);
            printed += fputs(normal_color, out);
        }

        // After match
        for (; *p && *p != '\r' && *p != '\n'; ++p)
            printed += fputc_column(out, *p, *p, &column);

        printed += fprintf(out, "\n");

        const char *eol = strchrnul(line, '\n');
        if (print_carets && start >= line && start < eol && line <= start) {
            for (int num = 0; num < digits; num++)
                printed += fputc(' ', out);
            printed += fputs(": ", out);
            int column = 0;
            for (const char *sp = line; *sp && *sp != '\n'; ++sp) {
                char print_char;
                if (sp < start)
                    print_char = ' ';
                else if (sp == start && sp == end)
                    print_char = '^';
                else if (sp >= start && sp < end)
                    print_char = '-';
                else
                    print_char = ' ';
                printed += fputc_column(out, *sp, print_char, &column);
            }
            printed += fputs("\n", out);
        }
    }
    fflush(out);
    return printed;
}

// vim: ts=4 sw=0 et cino=L2,l1,(0,W4,m1,\:0
