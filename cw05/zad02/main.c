#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "ptr_vector.h"

typedef struct {
    char *filename;
    char *date;
} pair_date;

typedef struct {
    char *filename;
    size_t size;
} pair_size;

const char *SEP = " \f\n\r\t\v";

int pair_date_cmp(const void *p1, const void *p2) {
    pair_date * const *pair1 = p1;
    pair_date * const *pair2 = p2;

    return strcmp((*pair2)->date, (*pair1)->date);
}

int pair_size_cmp(const void *p1, const void *p2) {
    pair_size * const *pair1 = p1;
    pair_size * const *pair2 = p2;

    return (*pair2)->size - (*pair1)->size;
}

pair_date *extract_w_date(char *line) {
    pair_date *result = malloc(sizeof(*result));

    char *tok = strtok(line, SEP);
    for (size_t i = 0; i < 5; i++) {
        tok = strtok(NULL, SEP);
    }

    char *date = strdup(tok);
    tok = strtok(NULL, SEP);
    char *time = strdup(tok);
    tok = strtok(NULL, SEP);

    asprintf(&result->date, "%s %s", date, time);
    free(date);
    free(time);

    result->filename = strdup(tok);
    return result;
};

pair_size *extract_w_size(char *line) {
    pair_size *result = malloc(sizeof(*result));

    char *tok = strtok(line, SEP);
    for (size_t i = 0; i < 4; i++) {
        tok = strtok(NULL, SEP);
    }

    sscanf(tok, "%zu", &result->size);

    for (size_t i = 0; i < 3; i++) {
        tok = strtok(NULL, SEP);
    }

    result->filename = strdup(tok);
    return result;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid argument count\n");
        return EXIT_FAILURE;
    }

    bool date;
    if (strcmp("date", argv[1]) == 0) {
        date = true;
    }
    else if (strcmp("size", argv[1]) == 0) {
        date = false;
    }
    else {
        fprintf(stderr, "malformed arguments\n");
        return EXIT_FAILURE;
    }

    FILE *pipe = popen("ls -la --time-style=long-iso", "r");
    if (!pipe) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    ptr_vector pairs;
    vec_init(&pairs);

    bool first = true;
    char *line = NULL;
    size_t n = 0;
    while (getline(&line, &n, pipe) > 0) {
        if (first) {
            first = false;
            continue;
        }

        vec_push_back(&pairs, date ? extract_w_date(line) : extract_w_size(line));
    }

    free(line);

    if (date) {
        qsort(pairs.storage, pairs.size, sizeof(*pairs.storage), pair_date_cmp);

        for (size_t i = 0; i < pairs.size; i++) {
            pair_date *pair = pairs.storage[i];
            puts(pair->filename);
        }

        while (pairs.size > 0) {
            pair_date *pair = vec_pop_back(&pairs);

            free(pair->filename);
            free(pair->date);
            free(pair);
        }
    }
    else {
        qsort(pairs.storage, pairs.size, sizeof(*pairs.storage), pair_size_cmp);

        for (size_t i = 0; i < pairs.size; i++) {
            pair_size *pair = pairs.storage[i];
            puts(pair->filename);
        }

        while (pairs.size > 0) {
            pair_size *pair = vec_pop_back(&pairs);

            free(pair->filename);
            free(pair);
        }
    }

    vec_clear(&pairs);
    pclose(pipe);
}