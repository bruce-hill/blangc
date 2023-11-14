#include <assert.h>
#include <gc.h>
#include <gc/cord.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/param.h>
#include <err.h>

#include "../SipHash/halfsiphash.h"
#include "types.h"
#include "array.h"
#include "string.h"

#define CLAMP(x, lo, hi) MIN(hi, MAX(x,lo))

extern const void *SSS_HASH_VECTOR;

typedef struct {
    uint8_t success;
    int32_t index;
} find_result_t;

static Str_t compacted(const Str_t str)
{
    if (str.stride == 1) return str;
    char *buf = GC_MALLOC_ATOMIC(str.length + 1);
    for (int64_t i = 0; i < str.length; i++)
        buf[i] = str.data[i*str.stride];
    return (Str_t){.data=buf, .length=str.length, .stride=1};
}

public CORD Str__cord(const Str_t *s, bool colorize, const Type *type)
{
    (void)type;
    char *data = s->data;
    // Note: it's important to have unicode strings not get broken up with
    // escapes, otherwise they won't print right.
    if (colorize) {
        CORD c = "\x1b[35m\"";
        for (uint32_t i = 0; i < s->length; i++) {
            switch (data[i]) {
#define BACKSLASHED(esc) "\x1b[35m\\\x1b[1m" esc "\x1b[0;34m"
            case '\a': c = CORD_cat(c, BACKSLASHED("a")); break;
            case '\b': c = CORD_cat(c, BACKSLASHED("b")); break;
            case '\x1b': c = CORD_cat(c, BACKSLASHED("e")); break;
            case '\f': c = CORD_cat(c, BACKSLASHED("f")); break;
            case '\n': c = CORD_cat(c, BACKSLASHED("n")); break;
            case '\r': c = CORD_cat(c, BACKSLASHED("r")); break;
            case '\t': c = CORD_cat(c, BACKSLASHED("t")); break;
            case '\v': c = CORD_cat(c, BACKSLASHED("v")); break;
            case '"': c = CORD_cat(c, BACKSLASHED("\"")); break;
            case '\\': c = CORD_cat(c, BACKSLASHED("\\")); break;
            case '\x00' ... '\x06': case '\x0E' ... '\x1A':
            case '\x1C' ... '\x1F': case '\x7F' ... '\x7F':
                CORD_sprintf(&c, "%r" BACKSLASHED("x%02X"), c, data[i]);
                break;
            default: c = CORD_cat_char(c, data[i]); break;
#undef BACKSLASHED
            }
        }
        c = CORD_cat(c, "\"\x1b[m");
        if (strcmp(type->name, "Str") != 0)
            CORD_sprintf(&c, "\x1b[0;1m%s::\x1b[m%r", type->name, c);
        return c;
    } else {
        CORD c = "\"";
        for (uint32_t i = 0; i < s->length; i++) {
            switch (data[i]) {
            case '\a': c = CORD_cat(c, "\\a"); break;
            case '\b': c = CORD_cat(c, "\\b"); break;
            case '\x1b': c = CORD_cat(c, "\\e"); break;
            case '\f': c = CORD_cat(c, "\\f"); break;
            case '\n': c = CORD_cat(c, "\\n"); break;
            case '\r': c = CORD_cat(c, "\\r"); break;
            case '\t': c = CORD_cat(c, "\\t"); break;
            case '\v': c = CORD_cat(c, "\\v"); break;
            case '"': c = CORD_cat(c, "\\\""); break;
            case '\\': c = CORD_cat(c, "\\\\"); break;
            case '\x00' ... '\x06': case '\x0E' ... '\x1A':
            case '\x1C' ... '\x1F': case '\x7F' ... '\x7F':
                CORD_sprintf(&c, "%r\\x%02X", c, data[i]);
                break;
            default: c = CORD_cat_char(c, data[i]); break;
            }
        }
        c = CORD_cat_char(c, '"');
        if (strcmp(type->name, "Str") != 0)
            CORD_sprintf(&c, "%s::%r", type->name, c);
        return c;
    }
}

public int32_t Str__compare(const Str_t *x, const Str_t *y)
{
    int64_t length = x->length < y->length ? x->length : y->length;
    for (int64_t i = 0; i < length; i++) {
        char xc = x->data[i], yc = y->data[i];
        if (xc != yc)
            return (xc > yc) ? 1 : -1;
    }
    return (x->length > y->length) - (x->length < y->length);
}

public bool Str__equal(const Str_t *x, const Str_t *y)
{
    return (Str__compare(x, y) == 0);
}

public int Str__hash(const Str_t *s, const Type *type)
{
    (void)type;
    if (s->length == 0 || !s->data) return 0;

    const char *data;
    if (s->stride == -1) {
        data = s->data - s->length + 1;
    } else if (s->stride == 1) {
        data = s->data;
    } else {
        char *buf = alloca(s->length);
        for (int64_t i = 0; i < s->length; i++)
            buf[i] = s->data[i*s->stride];
        data = buf;
    }

    uint32_t hash;
    halfsiphash(data, s->length, SSS_HASH_VECTOR, (uint8_t*)&hash, sizeof(hash));
    return hash;
}

public Str_t Str__uppercased(const Str_t s)
{
    char *s2 = GC_MALLOC_ATOMIC(s.length + 1);
    for (int64_t i = 0; i < s.length; i++)
        s2[i] = toupper(s.data[i*s.stride]);
    return (Str_t){.data=s2, .length=s.length, .stride=1};
}

public Str_t Str__lowercased(const Str_t s)
{
    char *s2 = GC_MALLOC_ATOMIC(s.length + 1);
    for (int64_t i = 0; i < s.length; i++)
        s2[i] = tolower(s.data[i*s.stride]);
    return (Str_t){.data=s2, .length=s.length, .stride=1};
}

public Str_t Str__capitalized(const Str_t s)
{
    char *s2 = GC_MALLOC_ATOMIC(s.length + 1);
    int64_t i;
    for (i = 0; i < s.length; i++) {
        if (isalpha(s.data[i*s.stride])) {
            s2[i] = toupper(s.data[i*s.stride]);
            ++i;
            break;
        } else {
            s2[i] = s.data[i*s.stride];
        }
    }
    for (; i < s.length; i++)
        s2[i] = s.data[i*s.stride];
    return (Str_t){.data=s2, .length=s.length, .stride=1};
}

public Str_t Str__titlecased(const Str_t s)
{
    char *s2 = GC_MALLOC_ATOMIC(s.length + 1);
    bool should_uppercase = true;
    for (int64_t i = 0; i < s.length; i++) {
        if (isalpha(s.data[i*s.stride])) {
            if (should_uppercase) {
                s2[i] = toupper(s.data[i*s.stride]);
                should_uppercase = false;
            } else {
                s2[i] = tolower(s.data[i*s.stride]);
            }
        } else {
            should_uppercase = true;
            s2[i] = s.data[i*s.stride];
        }
    }
    return (Str_t){.data=s2, .length=s.length, .stride=1};
}

public bool Str__starts_with(const Str_t s, const Str_t prefix)
{
    if (s.length < prefix.length) return false;
    for (int64_t i = 0; i < prefix.length; i++) {
        if (s.data[i*s.stride] != prefix.data[i*prefix.stride])
            return false;
    }
    return true;
}

public bool Str__ends_with(const Str_t s, const Str_t suffix)
{
    if (s.length < suffix.length) return false;
    for (int64_t i = 0; i < suffix.length; i++) {
        if (s.data[(s.length-suffix.length+i)*s.stride] != suffix.data[i*suffix.stride])
            return false;
    }
    return true;
}

public Str_t Str__without_prefix(const Str_t s, const Str_t prefix)
{
    if (s.length < prefix.length) return s;
    for (int64_t i = 0; i < prefix.length; i++) {
        if (s.data[i*s.stride] != prefix.data[i*prefix.stride])
            return s;
    }
    return (Str_t){
        .data=s.data + prefix.length*s.stride,
        .length=s.length - prefix.length,
        .stride=s.stride,
        .cow=0, .atomic=1,
    };
}

public Str_t Str__without_suffix(const Str_t s, const Str_t suffix)
{
    if (s.length < suffix.length) return s;
    for (int64_t i = 0; i < suffix.length; i++) {
        if (s.data[(s.length - suffix.length + i)*s.stride] != suffix.data[i*suffix.stride])
            return s;
    }
    return (Str_t){
        .data=s.data,
        .length=s.length - suffix.length,
        .stride=s.stride,
        .cow=0, .atomic=1,
    };
}

public Str_t Str__trimmed(const Str_t s, const Str_t trim_chars, bool trim_left, bool trim_right)
{
    int64_t length = s.length;
    int64_t start = 0;
    if (trim_left) {
        for (; start < s.length; start++) {
            for (int64_t t = 0; t < trim_chars.length; t++) {
                if (s.data[start*s.stride] == trim_chars.data[t*trim_chars.stride])
                    goto found_ltrim;
            }
            goto done_trimming_left;
          found_ltrim:
            --length;
        }
    }
  done_trimming_left:;
    if (trim_right) {
        while (length > 0) {
            for (int64_t t = 0; t < trim_chars.length; t++) {
                if (s.data[(start+length-1)*s.stride] == trim_chars.data[t*trim_chars.stride])
                    goto found_rtrim;
            }
            goto done_trimming_right;
          found_rtrim:
            --length;
        }
    }
  done_trimming_right:;
    return (Str_t){.data=s.data+start*s.stride, .length=length, .stride=s.stride};
}

public const char *Str__c_string(const Str_t str)
{
    if (str.length == 0)
        return "";
    else if (str.stride == 1
        // Verify that the '\0' would be on the same page, so unlikely to segfault:
        && (((uint64_t)str.data + str.length + 1) & 0xFFF) != 0
        // Check for nul-termination
        && str.data[str.length+1] == '\0')
        return str.data;

    char *buf = GC_MALLOC_ATOMIC(str.length + 1);
    for (int64_t i = 0; i < str.length; i++)
        buf[i] = str.data[i*str.stride];
    buf[str.length] = '\0';
    return buf;
}

public Str_t Str__from_c_string(const char *str)
{
    size_t length = str ? strlen(str) : 0;
    if (length == 0) return (Str_t){.length=0, .stride=0};
    char *buf = GC_MALLOC_ATOMIC(length + 1);
    memcpy(buf, str, length+1);
    buf[length+1] = '\0';
    return (Str_t){.data=buf, .length=length, .stride=1};
}

public find_result_t Str__find(const Str_t str, const Str_t pat)
{
    if (str.length < pat.length) return (find_result_t){.success=0};
    if (pat.length == 0) return (find_result_t){.success=1, .index=1};

    // For short strings, do naive approach:
    // if (str.length*pat.length < UCHAR_MAX) {
    for (int64_t s = 0; s < str.length; s++) {
        for (int64_t p = 0; p < pat.length; p++) {
            if (str.data[s*str.stride] != pat.data[p*pat.stride])
                goto not_a_match;
        }
        return (find_result_t){.success=1, .index=s+1};
      not_a_match:;
    }
    return (find_result_t){.success=0};
    // }

    // // Boyer-Moore algorithm:
    // static int skip[UCHAR_MAX];
    // for (int32_t i = 0; i <= UCHAR_MAX; ++i)
    //     skip[i] = str.length;
    // for (int32_t i = 0; i < pat.length; ++i)
    //     skip[(unsigned char)pat.data[i*pat.stride]] = pat.length - i - 1;
    // char lastpatchar = pat.data[(pat.length - 1)*pat.stride];
    // int32_t min_skip = pat.length;
    // for (int32_t p = 0; p < pat.length - 1; ++p) {
    //     if (pat.data[p*pat.stride] == lastpatchar)
    //         min_skip = pat.length - p - 1;
    // }

    // for (int32_t i = pat.length - 1; i < str.length; ) {
    //     // Use skip table:
    //     int32_t can_skip = skip[(unsigned char)str.data[i*str.stride]];
    //     if (can_skip != 0) {
    //         i += can_skip;
    //         continue;
    //     }
    //     // Check for exact match:
    //     for (int32_t j = 0; j < pat.length; j++) {
    //         if (str.data[(i-pat.length+j)*str.stride] != pat.data[j*pat.stride]) {
    //             // Mismatch:
    //             i += min_skip;
    //             goto keep_going;
    //         }
    //     }
    //     return i - pat.length + 1;
    //   keep_going: continue;
    // }
    // return 0;
}

public Str_t Str__replace(Str_t text, Str_t pat, Str_t replacement, int64_t limit)
{
    text = compacted(text);
    pat = compacted(pat);
    replacement = compacted(replacement);
    char *buf;
    size_t size;
    FILE *mem = open_memstream(&buf, &size);
    for (const char *pos = text.data; ; --limit) {
        const char *match = limit == 0 ? NULL : memmem(pos, (size_t)text.length - (size_t)(pos - text.data), pat.data, pat.length);
        if (match) {
            fwrite(pos, 1, (size_t)(match - pos), mem);
            fwrite(replacement.data, 1, replacement.length, mem);
            pos = match + pat.length;
        } else {
            fwrite(pos, 1, (size_t)text.length - (size_t)(pos - text.data), mem);
            break;
        }
    }
    fflush(mem);
    char *str = GC_MALLOC_ATOMIC(size + 1);
    memcpy(str, buf, size+1);
    fclose(mem);
    free(buf);
    return (Str_t){.data=str, .length=size, .stride=1};
}

public Str_t Str__quoted(const Str_t text, const char *dsl, bool colorize)
{
    char *buf;
    size_t size;
    FILE *mem = open_memstream(&buf, &size);
    if (colorize) fputs("\x1b[35m", mem);
    if (dsl && dsl[0]) fprintf(mem, "$%s", dsl);
    const char *escape_color = colorize ? "\x1b[1;34m" : "";
    const char *reset_color = colorize ? "\x1b[0;35m" : "";
    fputc('"', mem);
    for (int64_t i = 0; i < text.length; i++) {
        char c = text.data[i*text.stride];
        switch (c) {
        case '\\': fprintf(mem, "%s\\\\%s", escape_color, reset_color); break;
        case '"': fprintf(mem, "%s\\\"%s", escape_color, reset_color); break;
        case '\n': fprintf(mem, "%s\\n%s", escape_color, reset_color); break;
        case '\t': fprintf(mem, "%s\\t%s", escape_color, reset_color); break;
        case '\r': fprintf(mem, "%s\\r%s", escape_color, reset_color); break;
        case '\a': fprintf(mem, "%s\\a%s", escape_color, reset_color); break;
        case '\b': fprintf(mem, "%s\\b%s", escape_color, reset_color); break;
        case '\v': fprintf(mem, "%s\\v%s", escape_color, reset_color); break;
        default: {
            if (isprint(c))
                fputc(c, mem);
            else
                fprintf(mem, "%s\\x%02X%s", escape_color, (int)c, reset_color);
        }
        }
    }
    fputc('"', mem);
    if (colorize) fputs("\x1b[m", mem);
    fflush(mem);
    char *str = GC_MALLOC_ATOMIC(size + 1);
    memcpy(str, buf, size+1);
    fclose(mem);
    free(buf);
    return (Str_t){.data=str, .length=size, .stride=1};
}

public Str_Array_t Str__split(const Str_t str, const Str_t split_chars)
{
    if (str.length == 0) return (Str_Array_t){.stride=sizeof(Str_t)};
    Str_Array_t strings = {.stride=sizeof(Str_t)};
    size_t capacity = 0;
    bool separators[256] = {0};
    for (int64_t i = 0; i < split_chars.length; i++)
        separators[(int)split_chars.data[split_chars.stride*i]] = true;

    for (int64_t i = 0; i < str.length; i++) {
        if (separators[(int)str.data[str.stride*i]]) continue;
        int64_t length = 0;
        while (i < str.length && !separators[(int)str.data[str.stride*i]]) {
            ++length;
            ++i;
        }
        strings.data = GC_REALLOC(strings.data, sizeof(Str_t)*(capacity += 1));
        strings.data[strings.length++] = (Str_t){
            .data=&str.data[str.stride*(i-length)],
            .length=length, 
            .stride=str.stride,
        };
    }
    return strings;
}

public Str_t Str__join(Str_t glue, Str_Array_t pieces)
{
    if (pieces.length == 0) return (Str_t){.stride=1};

    int64_t length = 0;
    for (int64_t i = 0; i < pieces.length; i++) {
        if (i > 0) length += glue.length;
        length += ((Str_t*)((void*)pieces.data + i*pieces.stride))->length;
    }
    char *data = GC_MALLOC_ATOMIC((size_t)length+1);
    char *ptr = data;
    for (int64_t i = 0; i < pieces.length; i++) {
        if (i > 0) {
            for (int64_t j = 0; j < glue.length; j++)
                *(ptr++) = glue.data[j*glue.stride];
        }
        Str_t piece = *(Str_t*)((void*)pieces.data + i*pieces.stride);
        for (int64_t j = 0; j < piece.length; j++)
            *(ptr++) = piece.data[j*piece.stride];
    }
    return (Str_t){.data=data, .length=length, .stride=1};
}

public Type Str_type = {
    .name="Str",
    .size=sizeof(Str_t),
    .align=alignof(Str_t),
    .tag=VTableInfo,
    .VTableInfo={
        .cord=(void*)Str__cord,
        .compare=(void*)Str__compare,
        .equal=(void*)Str__equal,
        .hash=(void*)Str__hash,
    },
};

static CORD CString_cord(const char **s, bool colorize, const Type *type)
{
    Str_t str = {.data=(char*)*s, .length=strlen(*s), .stride=1};
    return Str__cord(&str, colorize, type);
}

static uint32_t CString_hash(const char **s, const Type *type)
{
    (void)type;
    if (!s) return 0;
    uint32_t hash;
    halfsiphash(*s, strlen(*s)+1, SSS_HASH_VECTOR, (uint8_t*)&hash, sizeof(hash));
    assert(strlen(*s) > 0);
    return hash;
}

static uint32_t CString_compare(const char **x, const char **y, const Type *type)
{
    (void)type;
    return strcmp(*x, *y);
}

public Type CString_type = {
    .name="CString",
    .size=sizeof(char*),
    .align=alignof(char*),
    .tag=VTableInfo,
    .VTableInfo={
        .cord=(void*)CString_cord,
        .hash=(void*)CString_hash,
        .compare=(void*)CString_compare,
    },
};

static CORD Cord_cord(const CORD *c, bool colorize, const Type *type)
{
    const char *str = CORD_to_const_char_star(*c);
    return CString_cord(&str, colorize, type);
}

static uint32_t Cord_hash(const CORD *c, const Type *type)
{
    const char *str = CORD_to_const_char_star(*c);
    return CString_hash(&str, type);
}

static uint32_t Cord_compare(const char **x, const char **y, const Type *type)
{
    (void)type;
    return CORD_cmp(*x, *y);
}

public Type Cord_type = {
    .name="Cord",
    .size=sizeof(CORD),
    .align=alignof(CORD),
    .tag=VTableInfo,
    .VTableInfo={
        .cord=(void*)Cord_cord,
        .hash=(void*)Cord_hash,
        .compare=(void*)Cord_compare,
    },
};

// vim: ts=4 sw=0 et cino=L2,l1,(0,W4,m1,\:0
