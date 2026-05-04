#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#define TRIE_IMPLEMENTATION
#include "trie.h"

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec * 1e-6;
}

typedef struct { uint8_t *key; uint8_t len; } TR_word_t;

static int word_cmp(const void *a, const void *b) {
    const TR_word_t *wa = a, *wb = b;
    int mn = wa->len < wb->len ? wa->len : wb->len;
    int  r = memcmp(wa->key, wb->key, mn);
    return r ? r : (int)wa->len - (int)wb->len;
}

int main(int argc, char **argv) {
    if (argc < 2) return printf("Usage: %s <file>\n", argv[0]), 1;

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) return perror("open"), 1;
    struct stat st; fstat(fd, &st);
    size_t fsize = st.st_size;

    char *buf = mmap(NULL, fsize, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);
    if (buf == MAP_FAILED) return perror("mmap"), 1;
    madvise(buf, fsize, MADV_SEQUENTIAL);

    size_t max_words = fsize / 2 + 1;

    uint8_t   *norm_buf  = mmap(NULL, fsize, PROT_READ|PROT_WRITE,
                                MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    TR_word_t *words     = mmap(NULL, max_words * sizeof(TR_word_t),
                                PROT_READ|PROT_WRITE,
                                MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    int      word_count = 0;
    uint8_t *wp  = norm_buf;
    char    *p   = buf;
    char    *end = buf + fsize;

    while (p < end) {
        while (p < end && TR_lut[(uint8_t)*p] == 255) p++;
        if (p >= end) break;
        uint8_t *ws = wp;
        uint8_t  wl = 0;
        while (p < end && TR_lut[(uint8_t)*p] != 255)
            *wp++ = TR_lut[(uint8_t)*p++], wl++;
        words[word_count].key = ws;
        words[word_count].len = wl;
        word_count++;
    }

    qsort(words, word_count, sizeof(TR_word_t), word_cmp);

    uint8_t **norm_keys = mmap(NULL, max_words * sizeof(uint8_t *),
                               PROT_READ|PROT_WRITE,
                               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    uint8_t  *norm_lens = mmap(NULL, max_words,
                               PROT_READ|PROT_WRITE,
                               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    for (int i = 0; i < word_count; i++) {
        norm_keys[i] = words[i].key;
        norm_lens[i] = words[i].len;
    }

    printf("Words: %d\n", word_count);

    TR_trie_t *trie = TR_create(1000000);
    if (!trie) return printf("create failed\n"), 1;

    double t0 = get_time_ms();
    TR_insert_batch(trie, norm_keys, norm_lens, word_count);
    printf("Insert: %.4f ms\n", get_time_ms() - t0);

    t0 = get_time_ms();
    unsigned long long found = TR_get_batch(trie, norm_keys, norm_lens, word_count);
    printf("Get:    %.4f ms (found: %llu)\n", get_time_ms() - t0, found);

    TR_destroy(trie);
    munmap(norm_buf,  fsize);
    munmap(words,     max_words * sizeof(TR_word_t));
    munmap(norm_keys, max_words * sizeof(uint8_t *));
    munmap(norm_lens, max_words);
    munmap(buf, fsize);
    return 0;
}
