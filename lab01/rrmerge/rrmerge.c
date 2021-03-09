#include "rrmerge.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void free_main_vector(ptr_vector *main_vector) {
    for (int i = main_vector->size - 1; i >= 0; i--) {
        remove_row_block(main_vector, i);
    }
}

size_t add_row_block(ptr_vector *main_vector, FILE *merged_file) {
    ptr_vector *row_block = malloc(sizeof(*row_block));
    vec_init(row_block);

    char* lineptr = NULL;
    size_t bufsize = 0;
    while (getline(&lineptr, &bufsize, merged_file) != -1) {
        vec_push_back(row_block, lineptr);
        lineptr = NULL;
        bufsize = 0;
    }
    free(lineptr);

    vec_push_back(main_vector, row_block);
    return main_vector->size - 1;
}

void remove_row_block(ptr_vector *main_vector, size_t block_idx) {
    ptr_vector *row_block = vec_erase(main_vector, block_idx);

    while (row_block->size > 0) {
        free(vec_pop_back(row_block));
    }

    free(row_block);
}

void remove_row(ptr_vector *main_vector, size_t block_idx, size_t row_idx) {
    ptr_vector *row_block = main_vector->storage[block_idx];

    free(vec_erase(row_block, row_idx));
}

void add_file_pair(ptr_vector *file_pairs_vector, char *path_pair) {
    file_pair *pair = malloc(sizeof(*pair));
    char *colon_ptr = strchr(path_pair, ':');

    pair->path_a = strndup(path_pair, colon_ptr - path_pair);
    pair->path_b = strdup(&colon_ptr[1]);

    vec_push_back(file_pairs_vector, pair);
}

void merge_file_pairs(ptr_vector *tmp_files_vector, ptr_vector *file_pairs_vector) {
    for (int i = 0; i < file_pairs_vector->size; i++) {
        file_pair *pair = file_pairs_vector->storage[i];
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
            vec_push_back(tmp_files_vector, output);

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

void print_file_pairs(ptr_vector *file_pairs_vector) {
    for (int i = 0; i < file_pairs_vector->size; i++) {
        file_pair *pair = file_pairs_vector->storage[i];

        printf("%s merged with %s\n", pair->path_a, pair->path_b);
    }
}

void free_file_pairs(ptr_vector *file_pairs_vector) {
    while (file_pairs_vector->size > 0) {
        file_pair *pair = vec_pop_back(file_pairs_vector);

        free(pair->path_a);
        free(pair->path_b);
        free(pair);
    }
}

void free_tmp_files(ptr_vector *tmp_files_vector) {
    while (tmp_files_vector->size > 0) {
        fclose(vec_pop_back(tmp_files_vector));
    }
}
