#define TRIE_IMPLEMENTATION
#include "trie.h"

#include <stdio.h>
#include <time.h>
#include <string.h>

#define MAX_WORD_LEN 1024

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <input_file> [output_log_file]\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];
    FILE *file = fopen(file_path, "r");
    if (!file) {
        perror("Could not open input file");
        return 1;
    }

    FILE *out = stdout;
    if (argc >= 3) {
        out = fopen(argv[2], "a");
        if (!out) {
            perror("Could not open log file");
            fclose(file);
            return 1;
        }
    }

    TR_trie_t *trie = TR_create();
    if (!trie) {
        TR_error("Could not create trie");
        if (out != stdout) fclose(out);
        fclose(file);
        return 1;
    }

    char word[MAX_WORD_LEN];
    unsigned long long count = 0;
    clock_t start, end;

    #if defined(TR_USE_ARENA)
        fprintf(out, "Benchmarking (ARENA MODE): %s\n", file_path);
    #else
        fprintf(out, "Benchmarking (NO ARENA MODE): %s\n", file_path);
    #endif

    start = clock();
    while (fscanf(file, "%1023s", word) == 1) {
        if (TR_insert(trie, word) != 0) {
            TR_error("Could not insert word: %s", word);
            goto cleanup;
        }
        count++;
    }
    end = clock();
    fprintf(out, "Inserted %llu words: %f seconds\n", count, (double)(end - start) / CLOCKS_PER_SEC);

    rewind(file);
    start = clock();
    while (fscanf(file, "%1023s", word) == 1) {
        if (TR_get(trie, word) == NULL) { 
            TR_error("Could not get word: %s", word);
            goto cleanup;
        }
    }
    end = clock();
    fprintf(out, "Get %llu words: %f seconds\n", count, (double)(end - start) / CLOCKS_PER_SEC);

    rewind(file);
    start = clock();
    while (fscanf(file, "%1023s", word) == 1) {
        if (TR_delete(trie, word) != 0) {
            TR_error("Could not delete word: %s", word);
            goto cleanup;
        }
    }
    end = clock();
    fprintf(out, "Delete %llu words: %f seconds\n", count, (double)(end - start) / CLOCKS_PER_SEC);

cleanup:
    if (out != stdout) fclose(out);
    fclose(file);
    TR_destroy(trie);

    return 0;
}
