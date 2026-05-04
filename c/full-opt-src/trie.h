#ifndef TRIE_H
#define TRIE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define TR_ALPHABET_SIZE 26
#define TR_END_BIT       (1U << 31)
#define TR_INDEX_MASK    (~TR_END_BIT)

typedef uint32_t TR_node_t;

typedef struct {
    TR_node_t  hot_path[TR_ALPHABET_SIZE];
    TR_node_t *pool;
    uint32_t   pool_count;
    uint32_t   pool_capacity;
} TR_trie_t;

static const uint8_t TR_lut[256] = {
    ['a']=0, ['b']=1, ['c']=2, ['d']=3, ['e']=4, ['f']=5, ['g']=6, ['h']=7,
    ['i']=8, ['j']=9, ['k']=10, ['l']=11, ['m']=12, ['n']=13, ['o']=14, ['p']=15,
    ['q']=16, ['r']=17, ['s']=18, ['t']=19, ['u']=20, ['v']=21, ['w']=22, ['x']=23,
    ['y']=24, ['z']=25,
    ['A']=0, ['B']=1, ['C']=2, ['D']=3, ['E']=4, ['F']=5, ['G']=6, ['H']=7,
    ['I']=8, ['J']=9, ['K']=10, ['L']=11, ['M']=12, ['N']=13, ['O']=14, ['P']=15,
    ['Q']=16, ['R']=17, ['S']=18, ['T']=19, ['U']=20, ['V']=21, ['W']=22, ['X']=23,
    ['Y']=24, ['Z']=25,
    [0 ... '@']=255, ['{' ... 255]=255,
};

#if defined(TRIE_IMPLEMENTATION)

static int TR_grow(TR_trie_t *t)
{
    uint32_t nc  = t->pool_capacity * 2;
    size_t bytes = (size_t)nc * sizeof(TR_node_t);
    TR_node_t *p = mmap(NULL, bytes, PROT_READ|PROT_WRITE,
                        MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return 0;
    madvise(p, bytes, MADV_HUGEPAGE);
    memcpy(p, t->pool, (size_t)t->pool_count * sizeof(TR_node_t));
    memset(p + t->pool_count, 0,
           (size_t)(nc - t->pool_count) * sizeof(TR_node_t));
    munmap(t->pool, (size_t)t->pool_capacity * sizeof(TR_node_t));
    t->pool = p;
    t->pool_capacity = nc;
    return 1;
}

TR_trie_t *TR_create(uint32_t cap)
{
    TR_trie_t *t = malloc(sizeof(TR_trie_t));
    if (!t) return NULL;
    memset(t->hot_path, 0, sizeof(t->hot_path));
    if (cap < 4000000) cap = 4000000;
    size_t bytes = (size_t)cap * sizeof(TR_node_t);
    t->pool = mmap(NULL, bytes, PROT_READ|PROT_WRITE,
                   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (t->pool == MAP_FAILED) {
        free(t);
        return NULL;
    }
    madvise(t->pool, bytes, MADV_HUGEPAGE);
    memset(t->pool, 0, bytes);
    t->pool_count    = 1;
    t->pool_capacity = cap;
    return t;
}

void TR_destroy(TR_trie_t *t)
{
    if (!t) return;
    munmap(t->pool, (size_t)t->pool_capacity * sizeof(TR_node_t));
    free(t);
}

/* NOTE
 * Keys are pre-normalized: every byte is already 0-25, length is known.
 * Inner loop is branch-free on character validation — just pointer chasing.
 */
static inline void TR_insert(TR_trie_t *restrict t,
                             const uint8_t *restrict key, int len)
{
    TR_node_t *restrict pool = t->pool;
    TR_node_t *curr = &t->hot_path[key[0]];
    for (int i = 1; i < len; i++) {
        uint32_t val   = *curr;
        uint32_t b_idx = val & TR_INDEX_MASK;
        if (__builtin_expect(!b_idx, 0)) {
            if (__builtin_expect(
                        t->pool_count + TR_ALPHABET_SIZE > t->pool_capacity, 0)) {
                if (!TR_grow(t)) return;
                pool = t->pool;
            }
            b_idx = t->pool_count;
            *curr = b_idx | (val & TR_END_BIT);
            t->pool_count += TR_ALPHABET_SIZE;
        }
        __builtin_prefetch(&pool[b_idx + key[i + 1]], 1, 3);
        curr = &pool[b_idx + key[i]];
    }
    *curr |= TR_END_BIT;
}

static inline bool TR_get(TR_trie_t *restrict t,
                          const uint8_t *restrict key, int len)
{
    const TR_node_t *restrict pool = t->pool;
    const TR_node_t *curr = &t->hot_path[key[0]];
    for (int i = 1; i < len; i++) {
        uint32_t val   = *curr;
        uint32_t b_idx = val & TR_INDEX_MASK;
        if (!b_idx) return false;
        __builtin_prefetch(&pool[b_idx + key[i + 1]], 0, 3);
        curr = &pool[b_idx + key[i]];
    }
    return (*curr & TR_END_BIT) != 0;
}

static void TR_insert_batch(TR_trie_t *t,
                            uint8_t **keys, uint8_t *lens, int n)
{
    for (int i = 0; i < n; i++) {
        if (i + 6 < n) __builtin_prefetch(keys[i + 6], 0, 1);
        /* NOTE: hot_path is 104 bytes — always L1. Use its value to prefetch
           the FIRST pool block for the word 4 ahead, hiding the real miss. */
        if (i + 4 < n) {
            uint32_t b = t->hot_path[keys[i+4][0]] & TR_INDEX_MASK;
            if (b) __builtin_prefetch(&t->pool[b + keys[i+4][1]], 1, 2);
        }
        TR_insert(t, keys[i], lens[i]);
    }
}

static unsigned long long TR_get_batch(TR_trie_t *t,
                                       uint8_t **keys, uint8_t *lens, int n)
{
    unsigned long long found = 0;
    for (int i = 0; i < n; i++) {
        if (i + 6 < n) __builtin_prefetch(keys[i + 6], 0, 1);
        if (i + 4 < n) {
            uint32_t b = t->hot_path[keys[i+4][0]] & TR_INDEX_MASK;
            if (b) __builtin_prefetch(&t->pool[b + keys[i+4][1]], 0, 2);
        }
        found += TR_get(t, keys[i], lens[i]);
    }
    return found;
}

#endif
#endif
