#pragma once
#include "rrmerge_ptr_vector.h"
#include <stdio.h>

typedef struct {
    char *path_a;
    char *path_b;
} file_pair;

size_t add_row_block(ptr_vector *main_vector, FILE *merged_file);
void remove_row_block(ptr_vector *main_vector, size_t block_idx);
void remove_row(ptr_vector *main_vector, size_t block_idx, size_t row_idx);
void free_main_vector(ptr_vector *main_vector);

void add_file_pair(ptr_vector *file_pairs_vector, char *path_pair);
void merge_file_pairs(ptr_vector *tmp_files_vector, ptr_vector *file_pairs_vector);
void print_file_pairs(ptr_vector *file_pairs_vector);
void free_file_pairs(ptr_vector *file_pairs_vector);

void free_tmp_files(ptr_vector *tmp_files_vector);