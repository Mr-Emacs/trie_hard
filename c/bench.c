#define TRIE_IMPLEMENTATION
#include "trie.h"

#include <stdio.h>
#include <time.h>
#include <string.h>

#define MAX_WORD_LEN 1024

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <file_path>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];

    FILE *file = fopen(file_path, "r");
    if (!file) {
        perror("Could not open file");
        return 1;
    }

    TR_trie_t *trie = TR_create();
    if (!trie) {
        fprintf(stderr, "Failed to create trie\n");
        return 1;
    }

    char word[MAX_WORD_LEN];
    unsigned long long count = 0;
    clock_t start, end;

    #if defined(TR_USE_ARENA)
        printf("Benchmarking (ARENA MODE): %s\n", file_path);
    #else
        printf("Benchmarking (NO ARENA MODE): %s\n", file_path);
    #endif

    start = clock();

    while (fscanf(file, "%1023s", word) == 1) {
        TR_insert(trie, word);
        count++;
        if (count % 1000000 == 0) printf("Inserted %llu words...\n", count);
    }

    end = clock();
    printf("Inserted %llu words: %f seconds\n",
           count,
           (double)(end - start) / CLOCKS_PER_SEC);

    rewind(file);

    start = clock();

    while (fscanf(file, "%1023s", word) == 1) {
        TR_get(trie, word);
    }

    end = clock();
    printf("Get %llu words:      %f seconds\n",
           count,
           (double)(end - start) / CLOCKS_PER_SEC);

    /*
        NOTE:
        In arena mode, delete is NOT meaningful.
        Memory is not freed per-node; it's all freed at TR_destroy().
        So TR_delete is basically logical-only and does not reclaim memory.
    */
    rewind(file);

    start = clock();

    while (fscanf(file, "%1023s", word) == 1) {
        TR_delete(trie, word);
    }

    end = clock();
    printf("Delete %llu words (logical): %f seconds\n",
           count,
           (double)(end - start) / CLOCKS_PER_SEC);

    fclose(file);
    TR_destroy(trie);

    return 0;
}
