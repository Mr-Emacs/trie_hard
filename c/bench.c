#define TRIE_IMPLEMENTATION
#include "trie.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

#define MAX_WORD_LEN 1024

int main(int argc, char **argv) {
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
    char word[MAX_WORD_LEN];
    unsigned long long count = 0;
    clock_t start, end;

    printf("Benchmarking: %s\n", file_path);

    start = clock();
    while (fscanf(file, "%1023s", word) == 1) {
        TR_insert(trie, word);
        count++;
        if (count % 1000000 == 0) printf("Inserted %llu words...\n", count / 1000000);
    }
    end = clock();
    printf("Inserted %llu words: %f seconds\n", count, (double)(end - start) / CLOCKS_PER_SEC);

    rewind(file);
    start = clock();
    while (fscanf(file, "%1023s", word) == 1) {
        TR_get(trie, word);
    }
    end = clock();
    printf("Get %llu words:      %f seconds\n", count, (double)(end - start) / CLOCKS_PER_SEC);

    rewind(file);
    start = clock();
    while (fscanf(file, "%1023s", word) == 1) {
        TR_delete(trie, word);
    }
    end = clock();
    printf("Delete %llu words:   %f seconds\n", count, (double)(end - start) / CLOCKS_PER_SEC);

    fclose(file);
    TR_destroy(trie);
    return 0;
}
