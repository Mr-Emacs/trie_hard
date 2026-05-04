#ifndef TRIE_H
#define TRIE_H

/* NOTE(Mr-Segfault): Description
  This is a stb single header trie library for C. Simple by design.
  The error logging is internal so no need for users to manually call it.
  The error log call back can be customized via a custom call back to enable it
  just #define TR_CUSTOM_CALLBACK.
*/

/* Example
#define TRIE_IMPLEMENTATION
#include "trie.h"
#include <stdio.h>

int main(void)
{
    TR_trie_t *trie = TR_create();
    if (!trie) return 1;

    TR_insert(trie, "hello");
    TR_insert(trie, "helium");
    TR_insert(trie, "heat");

    if (TR_get(trie, "hello")) printf("hello found\n");
    if (!TR_get(trie, "heaven")) printf("heaven not found\n");

    if (TR_prefix_search(trie, "he") == TR_SUCCESS)
        printf("prefix he exists\n");

    TR_delete(trie, "helium");
    if (!TR_get(trie, "helium")) printf("helium deleted\n");

    TR_destroy(trie);

    return 0;
}
*/

#include <stdint.h>

// NOTE(Mr-Segfault): Default alphabet count
#define TR_ALPHABET_SIZE 26
#define TR_INT_SIZE sizeof(uint32_t)

// NOTE(Mr-Segfault): Forward declartion of structures;
typedef struct TR_trie_t TR_trie_t;
typedef struct TR_trie_node_t TR_trie_node_t;

// NOTE(Mr-Segfault): Thread-local error variable
typedef enum {
    TR_SUCCESS = 0,
    TR_ERR_OUT_OF_MEMORY,
    TR_ERR_INVALID_KEY,
    TR_ERR_NOT_FOUND,
    TR_ERR_NULL_ARGUMENT
} TR_error_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
    #include <threads.h>
    #define TR_THREAD_LOCAL thread_local
#elif defined(_MSC_VER)
    #define TR_THREAD_LOCAL __declspec(thread)
#else
    #define TR_THREAD_LOCAL __thread
#endif

extern TR_THREAD_LOCAL TR_error_t TR_errno;

struct TR_trie_node_t
{
    /* NOTE(Mr-Segfault): I was a bit lazy and doing bool is too much so
    / for now I will use int -1 to represent not end and 0 to represent end.
    */
    uint8_t is_end;
    uint32_t children_mask;
    TR_trie_node_t *children[TR_ALPHABET_SIZE];
};

struct TR_trie_t
{
    TR_trie_node_t *root;

#ifdef TR_USE_ARENA
    void *arena;
#endif
};

// NOTE(Mr-Segfault): Creates a trie.
// Returns a trie node if success returns NULL if failure. Error can be catched.
TR_trie_t *TR_create();

// NOTE(Mr-Segfault): Destroy's an existing trie.
void TR_destroy(TR_trie_t *trie);

// NOTE(Mr-Segfault): Inserts a key into an existing trie.
// return 0 if inserted. Returns -1 if it could not
// create a node to be inserted. Error can be catched with errno style error.
int TR_insert(TR_trie_t *trie, const char *key);

// NOTE(Mr-Segfault):  Gets an element from the trie.
// Return an existing node if found else returns NULL.
TR_trie_node_t *TR_get(TR_trie_t *trie, const char *key);

// NOTE(Mr-Segfault): Checks the existense of a prefix of chars in the trie
// Returns 0 if found else returns -1.
int TR_prefix_search(TR_trie_t *trie, const char *key);

// NOTE(Mr-Segfault): Delete a node from the trie using a key
// In arena mode, delete is NOT meaningful.
// Memory is not freed per-node; it's all freed at internal TR_destroy().
// So TR_delete is basically logical-only and does not reclaim memory.
int TR_delete(TR_trie_t *trie, const char *key);

// NOTE(@Mr-segfault): Prints the error message on call.
void TR_error(const char *fmt, ...);

#if defined(TRIE_IMPLEMENTATION)

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

TR_THREAD_LOCAL TR_error_t TR_errno = TR_SUCCESS;

#include <stdarg.h>

void TR_error(const char *fmt, ...)
{
    const char *err_str = "UNKNOWN_ERROR";
    switch (TR_errno) {
        case TR_SUCCESS:           err_str = "Success"; break;
        case TR_ERR_OUT_OF_MEMORY: err_str = "Out of memory"; break;
        case TR_ERR_INVALID_KEY:   err_str = "Invalid key (only a-z allowed)"; break;
        case TR_ERR_NOT_FOUND:     err_str = "Key not found"; break;
        case TR_ERR_NULL_ARGUMENT: err_str = "Null argument provided"; break;
    }

    fprintf(stderr, "[TRIE ERROR: %s] ", err_str);

    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
    
    fprintf(stderr, "\n");
}

#if defined(TR_USE_ARENA)

#define TR_CHUNK_SIZE (1024 * 1024)
#define TR_IS_ALLIGN(x, y) ((x & (y - 1)) == 0)
#define TR_MIN(x, y) (((x) < (y)) ? (x) : (y))
#define TR_MAX(x, y) (((x) > (y)) ? (x) : (y))

typedef struct TR_arena_chunk_t
{
    struct TR_arena_chunk_t *next;
    size_t pos;
    size_t capacity;
    uint8_t chunk[];
} TR_arena_chunk_t;

typedef struct
{
    TR_arena_chunk_t *head;
    TR_arena_chunk_t *current;
} TR_arena_t;

static TR_arena_t *TR_arena_create(void);
static void TR_arena_free(TR_arena_t *arena);
static void *TR_arena_push(TR_arena_t *arena, size_t size);
static void *TR_arena_push_zero(TR_arena_t *arena, size_t size);

static TR_arena_chunk_t *TR_chunk_create(size_t size)
{
    TR_arena_chunk_t *c = malloc(sizeof(TR_arena_chunk_t) + size);
    if (!c) { TR_errno = TR_ERR_OUT_OF_MEMORY; return NULL; }
    c->next = NULL;
    c->pos = 0;
    c->capacity = size;
    return c;
}

static TR_arena_t *TR_arena_create(void)
{
    TR_arena_t *a = malloc(sizeof(*a));
    if (!a) { TR_errno = TR_ERR_OUT_OF_MEMORY; return NULL; }

    a->head = TR_chunk_create(TR_CHUNK_SIZE);
    if (!a->head) { free(a); return NULL; }

    a->current = a->head;
    return a;
}

static void TR_arena_free(TR_arena_t *a)
{
    TR_arena_chunk_t *c = a->head;
    while (c) {
        TR_arena_chunk_t *n = c->next;
        free(c);
        c = n;
    }
    free(a);
}

static void *TR_arena_push_zero(TR_arena_t *a, size_t size)
{
    TR_arena_chunk_t *c = a->current;
    // Ensure 8-byte alignment for the next allocation
    size_t aligned_pos = (c->pos + 7) & ~7;

    if (aligned_pos + size > c->capacity) {
        // NOTE(@Mr-segfault): If the request is larger than default chunk
        // size it appropriately
        size_t next_size = (size > TR_CHUNK_SIZE) ? size : TR_CHUNK_SIZE;
        TR_arena_chunk_t *n = TR_chunk_create(next_size);
        if (!n) return NULL;
        
        c->next = n;
        a->current = n;
        c = n;
        aligned_pos = 0;
    }

    void *ptr = c->chunk + aligned_pos;
    c->pos = aligned_pos + size;
    memset(ptr, 0, size);
    return ptr;
}

#endif // TR_USE_ARENA

static void *TR_alloc(TR_trie_t *trie, size_t size)
{
#ifdef TR_USE_ARENA
    void *p = TR_arena_push_zero((TR_arena_t*)trie->arena, size);
    if (!p) TR_errno = TR_ERR_OUT_OF_MEMORY;
    return p;
#else
    void *p = calloc(1, size);
    if (!p) TR_errno = TR_ERR_OUT_OF_MEMORY;
    return p;
#endif
}

static TR_trie_node_t *TR_create_node(TR_trie_t *trie)
{
    return TR_alloc(trie, sizeof(TR_trie_node_t));
}

static void TR_free(TR_trie_t *trie, void *ptr)
{
#ifndef TR_USE_ARENA
    free(ptr);
#else
    (void)trie;
    (void)ptr;
#endif
}


TR_trie_t *TR_create(void)
{
    TR_trie_t *trie = calloc(1, sizeof(*trie));
    if (!trie) { TR_errno = TR_ERR_OUT_OF_MEMORY; return NULL; }

#ifdef TR_USE_ARENA
    trie->arena = TR_arena_create();
    if (!trie->arena) { free(trie); return NULL; }
#endif

    trie->root = TR_create_node(trie);
    if (!trie->root) return NULL;

    return trie;
}


#ifndef TR_USE_ARENA
static void TR_destroy_node(TR_trie_node_t *node)
{
    if (!node) return;
    // NOTE(@Mr-Segfault): Only traverse indices present in the mask.
    // (Performance Optimization)
    uint32_t mask = node->children_mask;
    for (int i = 0; mask; i++) {
        if (mask & (1u << i)) {
            TR_destroy_node(node->children[i]);
            mask &= ~(1u << i);
        }
    }
    free(node);
}
#endif

void TR_destroy(TR_trie_t *trie)
{
    if (!trie) return;

#ifdef TR_USE_ARENA
    TR_arena_free((TR_arena_t*)trie->arena);
#else
    TR_destroy_node(trie->root);
#endif

    free(trie);
}

int TR_insert(TR_trie_t *trie, const char *key)
{
    if (!trie || !key) { TR_errno = TR_ERR_NULL_ARGUMENT; return -1; }

    TR_trie_node_t *cur = trie->root;

    for (const char *p = key; *p; p++) {
        if (*p < 'a' || *p > 'z') { TR_errno = TR_ERR_INVALID_KEY; return -1; }
        uint32_t idx = (uint32_t)(*p - 'a');
        uint32_t bit = 1u << idx;

        if (!(cur->children_mask & bit)) {
            cur->children[idx] = (TR_trie_node_t*)TR_alloc(trie, sizeof(TR_trie_node_t));
            if (!cur->children[idx]) return -1;
            cur->children_mask |= bit;
        }
        cur = cur->children[idx];
    }
    cur->is_end = 1;
    return 0;
}

TR_trie_node_t *TR_get(TR_trie_t *trie, const char *key)
{
    TR_errno = TR_SUCCESS;
    if (!trie || !key) { TR_errno = TR_ERR_NULL_ARGUMENT; return NULL; }

    TR_trie_node_t *cur = trie->root;
    for (const char *p = key; *p; p++) {
        if (*p < 'a' || *p > 'z') { TR_errno = TR_ERR_INVALID_KEY; return NULL; }
        uint32_t idx = (uint32_t)(*p - 'a');
        if (!(cur->children_mask & (1u << idx))) { TR_errno = TR_ERR_NOT_FOUND; return NULL; }
        cur = cur->children[idx];
    }

    if (!cur->is_end) { TR_errno = TR_ERR_NOT_FOUND; return NULL; }
    return cur;
}

int TR_prefix_search(TR_trie_t *trie, const char *key)
{
    if (!trie || !key) { TR_errno = TR_ERR_NULL_ARGUMENT; return -1; }

    TR_trie_node_t *cur = trie->root;

    for (const char *p = key; *p; p++) {
        if (*p < 'a' || *p > 'z') { TR_errno = TR_ERR_INVALID_KEY; return -1; }
        uint32_t idx = (uint32_t)(*p - 'a');
        if (!(cur->children_mask & (1u << idx))) return -1;
        cur = cur->children[idx];
    }

    return 0;
}

#ifndef TR_USE_ARENA
static int TR_delete_recursive(TR_trie_node_t *node, const char *key)
{
    if (!node) return 0;

    if (*key) {
        uint32_t idx = (uint32_t)(*key - 'a');
        if (TR_delete_recursive(node->children[idx], key + 1)) {
            free(node->children[idx]);
            node->children[idx] = NULL;
            node->children_mask &= ~(1u << idx);
            return (node->children_mask == 0 && !node->is_end);
        }
    } else {
        node->is_end = 0;
        return (node->children_mask == 0);
    }
    return 0;
}
#else
// NOTE(@Mr-segfault): Helper for Arena mode: Just clear masks so we don't follow dead paths
static int TR_delete_logical_recursive(TR_trie_node_t *node, const char *key) {
    if (!node) return 0;

    if (*key) {
        uint32_t idx = (uint32_t)(*key - 'a');
        if (node->children_mask & (1u << idx)) {
            if (TR_delete_logical_recursive(node->children[idx], key + 1)) {
                // We don't free, we just "hide" the path
                node->children_mask &= ~(1u << idx);
            }
        }
    } else {
        node->is_end = 0;
    }
    //NOTE(@Mr-segfault): Tell parent: "I am now a dead end" if I have no children and am not a word end
    return (node->children_mask == 0 && !node->is_end);
}
#endif

int TR_delete(TR_trie_t *trie, const char *key) {
    if (!trie || !key || !*key) return TR_ERR_NOT_FOUND;

#ifdef TR_USE_ARENA
    TR_delete_logical_recursive(trie->root, key);
#else
    TR_delete_recursive(trie->root, key);
#endif

    return 0;
}

#endif // TRIE_IMPLEMENTATION

#endif //TRIE_H
