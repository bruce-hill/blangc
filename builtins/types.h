#pragma once
#include <gc/cord.h>
#include <stdbool.h>
#include <stdint.h>

#include "string.h"

typedef struct {
    const char *name, *type;
    void *value;
} NamespaceBinding;

struct Type;

typedef struct {
    enum { NoInfo, NamedInfo, PointerInfo, ArrayInfo, TableInfo } tag;
    union {
        struct {} NoInfo;
        struct {
            const char *name;
            struct Type *base;
        } NamedInfo;
        struct {
            struct Type *pointed;
        } PointerInfo;
        struct {
            struct Type *item;
        } ArrayInfo;
        struct {
            struct Type *key, *value;
            size_t entry_size, value_offset;
        } TableInfo;
    } __data;
} TypeInfo;

typedef struct Equality {
    enum { EqualityComparison, EqualityFunction, EqualityData } tag;
    union {
        struct {} EqualityComparison; // Default: use (generic_compare()!=0)
        struct {
            bool (*fn)(const struct Type*, const void*, const void*);
        } EqualityFunction;
        struct {
            size_t size;
        } EqualityData;
    } __data;
} Equality;

#define EqualityMethod(compare_tag, ...) ((Equality){.tag=Equality##compare_tag, .__data.Equality##compare_tag={__VA_ARGS__}})

typedef struct Cording {
    enum { CordNotImplemented, CordFunction, CordNamed, CordPointer, CordArray, CordTable } tag;
    union {
        struct {} CordNotImplemented;
        struct {
            CORD (*fn)(const void*, bool);
        } CordFunction;
        struct {} CordNamed;
        struct {
            const char *sigil, *null_str;
        } CordPointer;
        struct {} CordArray;
        struct {} CordTable;
    } __data;
} Cording;

#define CordMethod(cord_tag, ...) ((Cording){.tag=Cord##cord_tag, .__data.Cord##cord_tag={__VA_ARGS__}})

typedef struct Hashing {
    enum { HashNotImplemented, HashFunction, HashData, HashArray, HashTable } tag;
    union {
        struct {} HashNotImplemented;
        struct {
            uint32_t (*fn)(const void*);
        } HashFunction;
        struct {
            size_t size;
        } HashData;
        struct {
            struct Hashing *item;
        } HashArray;
        struct {
            size_t entry_size, value_offset;
            struct Hashing *key, *value;
        } HashTable;
    } __data;
} Hashing;

#define HashMethod(hash_tag, ...) ((Hashing){.tag=Hash##hash_tag, .__data.Hash##hash_tag={__VA_ARGS__}})

typedef struct Ordering {
    enum { OrderingFunction, OrderingData, OrderingArray, OrderingTable } tag;
    union {
        struct {
            int32_t (*fn)(const void*, const void*);
        } OrderingFunction;
        struct {
            size_t size;
        } OrderingData;
        struct {} OrderingArray;
        struct {} OrderingTable;
    } __data;
} Ordering;

#define OrderingMethod(compare_tag, ...) ((Ordering){.tag=Ordering##compare_tag, .__data.Ordering##compare_tag={__VA_ARGS__}})

typedef struct Type {
    string_t name;
    TypeInfo info;
    Equality equality;
    Ordering order;
    Hashing hash;
    Cording cord;
    NamespaceBinding *bindings;
} Type;

bool generic_equals(const Type *type, const void *x, const void *y);
CORD generic_cord(const Type *type, const void *obj, bool colorize);
uint32_t generic_hash(const Type *type, const void *obj);
int32_t generic_compare(const void *x, const void *y, const Type *type); // Type is last for compatibility with qsort_r()
