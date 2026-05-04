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

// NOTE(Mr-Segfault): Enum for error
typedef enum
{
    TR_SUCCESS,
    TR_CREATE_NODE_FAILURE,
    TR_GET_NULL_NODE_ERROR,
    TR_GET_NOT_NODE_ERROR,
} TR_error_t;

// NOTE(Mr-Segfault): This is a function pointer callback used by our trie structure
// To get the errors if we incounter any because I thought what would make a better api rather than keeping track of a global error var. Note that this function does exit if an error is found so be wary of calling this function.
// By default we have a default callback for error handeling if you
// wish to customize it you can enum you own custom call back with a macro
// #define TR_CUSTOM_CALLBACK.
typedef void (*TR_error_callback)(TR_error_t err, const char *context);

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
    TR_error_callback on_error;
};

// NOTE(Mr-Segfault): Creates a trie.
// Returns a trie node if success returns NULL if failure
TR_trie_t *TR_create();

// NOTE(Mr-Segfault): Destroy's an existing trie.
void TR_destroy(TR_trie_t *trie);

// NOTE(Mr-Segfault): Inserts a key into an existing trie.
// return TR_SUCESS if inserted. Returns TR_CREATE_NODE_FAILURE if it could not
// create a node to be inserted
TR_error_t TR_insert(TR_trie_t *trie, const char *key);

// NOTE(Mr-Segfault):  Gets an element from the trie.
// Return an existing node if found else returns NULL.
TR_trie_node_t *TR_get(TR_trie_t *trie, const char *key);

// NOTE(Mr-Segfault): Checks the existense of a prefix of chars in the trie
// Returns TS_SUCCESS if found else returns TR_GET_NULL_NODE_ERROR.
TR_error_t TR_prefix_search(TR_trie_t *trie, const char *key);

// NOTE(Mr-Segfault): Delete a node from the trie using a key
TR_error_t TR_delete(TR_trie_t *trie, const char *key);

#if defined(TRIE_IMPLEMENTATION)

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define TR_NODE_POOL_SIZE 10000
static TR_trie_node_t *node_pool = NULL;
static size_t pool_ptr = 0;

static void TR_default_error_handler(TR_error_t err, const char *ctx)
{
    fprintf(stderr, "[TRIE ERROR %d] %s\n", err, ctx ? ctx : "");
}

static TR_trie_node_t *TR_create_node(void)
{
    TR_trie_node_t *node = calloc(1, sizeof(TR_trie_node_t));
    if (!node) return NULL;

    node->is_end = 0;
    return node;
}

static void TR_report(TR_trie_t *trie, TR_error_t err, const char *ctx)
{
    if (trie && trie->on_error)
        trie->on_error(err, ctx);
}

static TR_trie_t *TR_create_cb(TR_error_callback cb)
{
    TR_trie_t *trie = calloc(1, sizeof(TR_trie_t));
    if (!trie) return NULL;

    trie->root = TR_create_node();
    if (!trie->root)
    {
        free(trie);
        return NULL;
    }

#ifdef TR_CUSTOM_CALLBACK
    trie->on_error = cb;
#else
    (void)cb;
    trie->on_error = TR_default_error_handler;
#endif

    return trie;
}

TR_trie_t *TR_create(void)
{
    return TR_create_cb(0);
}
void TR_destroy_node(TR_trie_node_t *node)
{
    if (!node) return;
    // NOTE(@Mr-Segfault): Only traverse indices present in the mask (Performance Optimization)
    uint32_t mask = node->children_mask;
    for (int i = 0; mask > 0; i++) {
        if (mask & (1u << i)) {
            TR_destroy_node(node->children[i]);
            mask &= ~(1u << i);
        }
    }
    free(node);
}


void TR_destroy(TR_trie_t *trie)
{
    if (!trie) return;

    TR_destroy_node(trie->root);
    free(trie);
}

TR_error_t TR_insert(TR_trie_t *trie, const char *key) {
    if (!trie || !key) {
        TR_report(trie, TR_CREATE_NODE_FAILURE, "insert: null input");
        return TR_CREATE_NODE_FAILURE;
    }

    if (!trie || !key) return TR_CREATE_NODE_FAILURE;

    TR_trie_node_t *cur = trie->root;
    for (size_t i = 0; key[i] != '\0'; i++) {
        if (key[i] < 'a' || key[i] > 'z') {
            TR_report(trie, TR_CREATE_NODE_FAILURE, "insert: invalid char");
            return TR_CREATE_NODE_FAILURE;
        }

        uint32_t idx = (uint32_t)(key[i] - 'a');
        if (!(cur->children_mask & (1u << idx))) {
            cur->children[idx] = TR_create_node();
            if (!cur->children[idx])  {
                TR_report(trie, TR_CREATE_NODE_FAILURE, "alloc: Ccould not create");
                return TR_CREATE_NODE_FAILURE;
            }
            cur->children_mask |= (1u << idx);
        }
        cur = cur->children[idx];
    }
    cur->is_end = 1;
    return TR_SUCCESS;
}

TR_trie_node_t *TR_get(TR_trie_t *trie, const char *key)
{
    if (!trie || !key) {
        TR_report(trie, TR_GET_NULL_NODE_ERROR, "get: null input");
        return NULL;
    }

    TR_trie_node_t *cur = trie->root;

    for (size_t i = 0; key[i] != '\0'; i++) {
        uint32_t idx = (uint32_t)(key[i] - 'a');
        if (!(cur->children_mask & (1u << idx))) {
            return NULL;
        }
        cur = cur->children[idx];
    }
    return cur->is_end ? cur : NULL;

}

TR_error_t TR_prefix_search(TR_trie_t *trie, const char *key)
{
    if (!trie || !key) {
        TR_report(trie, TR_GET_NULL_NODE_ERROR, "prefix: null input");
        return TR_GET_NULL_NODE_ERROR;
    }

    TR_trie_node_t *cur = trie->root;

    for (size_t i = 0; key[i]; i++)
    {
        uint32_t idx = (uint32_t)(key[i] - 'a');
        if (idx < 0 || idx >= TR_ALPHABET_SIZE || !cur->children[idx])
            return TR_GET_NULL_NODE_ERROR;

        cur = cur->children[idx];
    }
    return TR_SUCCESS;
}

static int TR_delete_rec(TR_trie_node_t *node, const char *key, size_t depth)
{
    if (!node) return 0;

    if (key[depth] == '\0')
        node->is_end = 0;
    else
    {
        uint32_t idx = (uint32_t)(key[depth] - 'a');

        if (!(node->children_mask & (1u << idx))) return 0;
        if (TR_delete_rec(node->children[idx], key, depth + 1)) {
            free(node->children[idx]);
            node->children[idx] = NULL;
            node->children_mask &= ~(1u << idx);
        }
    }

    return (node->is_end == 0 && node->children_mask == 0);
}


TR_error_t TR_delete(TR_trie_t *trie, const char *key)
{
    if (!trie || !key || !trie->root) {
        TR_report(trie, TR_GET_NULL_NODE_ERROR, "delete: null input");
        return TR_GET_NULL_NODE_ERROR;
    }

    // NOTE(@Mr-Segfault): We don't delete the root node itself in this implementation
    TR_delete_rec(trie->root, key, 0);
    return TR_SUCCESS;
}


#endif

#endif //TRIE_H
