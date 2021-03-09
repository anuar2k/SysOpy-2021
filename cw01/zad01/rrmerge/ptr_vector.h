#pragma once
#include <stddef.h>

typedef struct {
    void **storage;
    size_t size;
    size_t capacity;
} ptr_vector;

void vec_init(ptr_vector *v);
void vec_clear(ptr_vector *v);

void vec_insert(ptr_vector *v, size_t at, void *value);
void vec_push_back(ptr_vector *v, void *value);

void *vec_erase(ptr_vector *v, size_t at);
void *vec_pop_back(ptr_vector *v);
