#include "rrmerge.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

size_t add_row_block(v_v_char *row_blocks, FILE *merged_file) {
    v_char *row_block = malloc(sizeof(*row_block));
    vec_init(row_block);

    char* lineptr = NULL;
    size_t bufsize = 0;
    while (getline(&lineptr, &bufsize, merged_file) != -1) {
        vec_push_back(row_block, lineptr);
        lineptr = NULL;
        bufsize = 0;
    }
    free(lineptr);

    vec_push_back(row_blocks, row_block);
    return row_blocks->size - 1;
}

void remove_row_block(v_v_char *row_blocks, size_t block_idx) {
    v_char *row_block = vec_erase(row_blocks, block_idx);

    while (row_block->size > 0) {
        free(vec_pop_back(row_block));
    }

    free(row_block);
}

void remove_row(v_v_char *row_blocks, size_t block_idx, size_t row_idx) {
    v_char *row_block = row_blocks->storage[block_idx];

    free(vec_erase(row_block, row_idx));
}

void print_row_blocks(v_v_char *row_blocks) {
    for (size_t i = 0; i < row_blocks->size; i++) {
        v_char *row_block = row_blocks->storage[i];
        printf("row block %zu:\n", i);

        for (size_t j = 0; j < row_block->size; j++) {
            char *row = row_block->storage[j];
            fputs(row, stdout);
        }
    }
}

void free_row_blocks(v_v_char *row_blocks) {
    for (size_t i = row_blocks->size; i-- > 0;) {
        remove_row_block(row_blocks, i);
    }
}

void add_file_pair(v_file_pair *file_pairs, char *path_pair) {
    file_pair *pair = malloc(sizeof(*pair));
    char *colon_ptr = strchr(path_pair, ':');

    pair->path_a = strndup(path_pair, colon_ptr - path_pair);
    pair->path_b = strdup(&colon_ptr[1]);

    vec_push_back(file_pairs, pair);
}

void merge_file_pairs(v_FILE *tmp_files, v_file_pair *file_pairs) {
    for (int i = 0; i < file_pairs->size; i++) {
        file_pair *pair = file_pairs->storage[i];
        FILE *input_a = fopen(pair->path_a, "r");
        FILE *input_b = fopen(pair->path_b, "r");
        FILE *output = tmpfile();

        if (input_a && input_b && output) {
            char* lineptr_a = NULL;
            char* lineptr_b = NULL;
            size_t bufsize_a = 0;
            size_t bufsize_b = 0;
            ssize_t read_a;
            ssize_t read_b;

            while ((read_a = getline(&lineptr_a, &bufsize_a, input_a)) != -1 
                && (read_b = getline(&lineptr_b, &bufsize_b, input_b)) != -1) {
                fwrite(lineptr_a, sizeof(*lineptr_a), read_a, output);
                fwrite(lineptr_b, sizeof(*lineptr_b), read_b, output);
            }

            rewind(output);
            vec_push_back(tmp_files, output);

            free(lineptr_a);
            free(lineptr_b);
        }
        else {
            fclose(output);
        }

        if (input_a) fclose(input_a);
        if (input_b) fclose(input_b);
    }
}

void free_file_pairs(v_file_pair *file_pairs) {
    while (file_pairs->size > 0) {
        file_pair *pair = vec_pop_back(file_pairs);

        free(pair->path_a);
        free(pair->path_b);
        free(pair);
    }
}

void free_tmp_files(v_FILE *tmp_files) {
    while (tmp_files->size > 0) {
        fclose(vec_pop_back(tmp_files));
    }
}
