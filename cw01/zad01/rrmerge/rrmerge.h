#pragma once
#include "rrmerge_ptr_vector.h"
#include <stdio.h>

typedef struct {
    char *path_a;
    char *path_b;
} file_pair;

//vector of vectors of char*
typedef ptr_vector v_v_char;
//vector of char*
typedef ptr_vector v_char;
//vector of file_pair*
typedef ptr_vector v_file_pair;
//vector of FILE*
typedef ptr_vector v_FILE;

size_t add_row_block(v_v_char *main, FILE *merged_file);
void remove_row_block(v_v_char *main, size_t block_idx);
void remove_row(v_v_char *main, size_t block_idx, size_t row_idx);
void free_main(v_v_char *main);

void add_file_pair(v_file_pair *file_pairs, char *path_pair);
void merge_file_pairs(v_FILE *tmp_files, v_file_pair *file_pairs);
void print_file_pairs(v_file_pair *file_pairs);
void free_file_pairs(v_file_pair *file_pairs);

void free_tmp_files(v_FILE *tmp_files);
