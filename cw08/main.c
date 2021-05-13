#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/times.h>
#include <pthread.h>
#include <signal.h>

#define S(x) #x
#define CEIL_DIV(a, b)     \
({                         \
    typeof(a) _a = (a);    \
    typeof(b) _b = (b);    \
    _a / _b + !!(_a % _b); \
})

//see man isspace
#define WHITESPACE_DELIM " \f\n\r\t\v"
#define PGM_MAGIC "P2"

typedef struct {
    size_t w;
    size_t h;
    uint8_t max_pixel_val;
    uint8_t *data;
} bitmap;

typedef struct {
    size_t pixel_from;
    size_t pixel_to;
    bitmap *in_bitmap;
    bitmap *out_bitmap;
} block_args;

typedef struct {
    uint8_t color_from;
    uint8_t color_to;
    bitmap *in_bitmap;
    bitmap *out_bitmap;
} numbers_args;

bool parse_in(FILE *in, bitmap *in_bitmap);
bool write_out(FILE *out, bitmap *out_bitmap);
bool process_bitmap(bool block, size_t thread_count, bitmap *in_bitmap, bitmap *out_bitmap);
void *block_worker(void *args);
void *numbers_worker(void *args);
char *read_in(FILE *in);
long extract_nanos(struct timespec *stamp);

int main(int argc, char **argv) {
    int result = EXIT_FAILURE;
    FILE *in = NULL;
    FILE *out = NULL;
    bitmap in_bitmap = {
        .data = NULL
    };
    bitmap out_bitmap = {
        .data = NULL
    };

    if (argc != 5) {
        fprintf(stderr, "invalid argument count\n");
        goto cleanup;
    }

    size_t thread_count;
    if (sscanf(argv[1], "%zu", &thread_count) != 1 || thread_count < 1) {
        fprintf(stderr, "invalid thread count\n");
        goto cleanup;
    }

    bool block;
    if (strcasecmp("numbers", argv[2]) == 0) {
        block = false;
    }
    else if (strcasecmp("block", argv[2]) == 0) {
        block = true;
    }
    else {
        fprintf(stderr, "invalid split type\n");
        goto cleanup;
    }

    in = fopen(argv[3], "r");
    if (!in) {
        perror(S(in));
        goto cleanup;
    }
    out = fopen(argv[4], "w");
    if (!out) {
        perror(S(out));
        goto cleanup;
    }

    if (!parse_in(in, &in_bitmap)) {
        fprintf(stderr, S(parse_in) " fail\n");
        goto cleanup;
    }

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    if (!process_bitmap(block, thread_count, &in_bitmap, &out_bitmap)) {
        fprintf(stderr, S(process_bitmap) " fail\n");
        goto cleanup;
    }

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    float result_time = (extract_nanos(&end_time) - extract_nanos(&start_time)) / 1e9;

    if (!write_out(out, &out_bitmap)) {
        fprintf(stderr, S(write_out) " fail\n");
        goto cleanup;
    }

    printf("CLOCKS_PER_SEC: %ld, result time: %f\n", CLOCKS_PER_SEC, result_time);

    result = EXIT_SUCCESS;

    cleanup:
    free(in_bitmap.data);
    free(out_bitmap.data);

    if (in)  fclose(in);
    if (out) fclose(out);

    return result;
}

bool parse_in(FILE *in, bitmap *in_bitmap) {
    bool result = false;
    char *content = read_in(in);

    char *curr = strtok(content, WHITESPACE_DELIM);
    if (!curr || strcmp(PGM_MAGIC, curr) != 0) goto cleanup;

    curr = strtok(NULL, WHITESPACE_DELIM);
    if (!curr || sscanf(curr, "%zu", &in_bitmap->w) != 1) goto cleanup;
    if (in_bitmap->w < 1) goto cleanup;

    curr = strtok(NULL, WHITESPACE_DELIM);
    if (!curr || sscanf(curr, "%zu", &in_bitmap->h) != 1) goto cleanup;
    if (in_bitmap->w < 1) goto cleanup;

    curr = strtok(NULL, WHITESPACE_DELIM);
    if (!curr || sscanf(curr, "%hhu", &in_bitmap->max_pixel_val) != 1) goto cleanup;
    if (in_bitmap->max_pixel_val < 1) goto cleanup;

    size_t pixel_count = in_bitmap->w * in_bitmap->h;
    in_bitmap->data = malloc(pixel_count * sizeof(*in_bitmap->data));

    for (size_t i = 0; i < pixel_count; i++) {
        curr = strtok(NULL, WHITESPACE_DELIM);
        if (sscanf(curr, "%hhu", &in_bitmap->data[i]) != 1) goto cleanup;
    }

    result = true;

    cleanup:
    free(content);
    return result;
}

bool write_out(FILE *out, bitmap *out_bitmap) {
    fprintf(out, PGM_MAGIC "\n%zu %zu\n%hhu\n", out_bitmap->w, out_bitmap->h, out_bitmap->max_pixel_val);

    size_t i = 0;
    for (size_t h = 0; h < out_bitmap->h; h++) {
        for (size_t w = 0; w < out_bitmap->w; w++) {
            fprintf(out, w == 0 ? "%hhu" : " %hhu", out_bitmap->data[i++]);
        }
        fputc('\n', out);
    }

    return true;
}

bool process_bitmap(bool block, size_t thread_count, bitmap *in_bitmap, bitmap *out_bitmap) {
    bool result = false;

    out_bitmap->w = in_bitmap->w;
    out_bitmap->h = in_bitmap->h;
    out_bitmap->max_pixel_val = in_bitmap->max_pixel_val;

    size_t pixel_count = out_bitmap->w * out_bitmap->h;
    out_bitmap->data = malloc(pixel_count * sizeof(*out_bitmap->data));

    pthread_t invalid_thread = pthread_self();
    pthread_t *workers = malloc(thread_count * sizeof(*workers));

    for (size_t i = 0; i < thread_count; i++) {
        workers[i] = invalid_thread;
    }

    void *worker_args;
    if (block) {
        block_args *block_args = malloc(thread_count * sizeof(*block_args));
        worker_args = block_args;

        for (size_t i = 0; i < thread_count; i++) {
            block_args[i].pixel_from = CEIL_DIV(i * pixel_count, thread_count);
            block_args[i].pixel_to = CEIL_DIV((i + 1) * pixel_count, thread_count) - 1;
            block_args[i].in_bitmap = in_bitmap;
            block_args[i].out_bitmap = out_bitmap;

            if (pthread_create(&workers[i], NULL, block_worker, &block_args[i]) != 0) {
                workers[i] = invalid_thread;
                goto cleanup;
            }
        }
    }
    else {
        numbers_args *numbers_args = malloc(thread_count * sizeof(*numbers_args));
        worker_args = numbers_args;

        for (size_t i = 0; i < thread_count; i++) {
            numbers_args[i].color_from = CEIL_DIV(i * (in_bitmap->max_pixel_val + 1), thread_count);
            numbers_args[i].color_to = CEIL_DIV((i + 1) * (in_bitmap->max_pixel_val + 1), thread_count) - 1;
            numbers_args[i].in_bitmap = in_bitmap;
            numbers_args[i].out_bitmap = out_bitmap;

            if (pthread_create(&workers[i], NULL, numbers_worker, &numbers_args[i]) != 0) {
                workers[i] = invalid_thread;
                goto cleanup;
            }
        }
    }

    for (size_t i = 0; i < thread_count; i++) {
        void *retval_ignore;

        if (pthread_join(workers[i], &retval_ignore) != 0) {
            goto cleanup;
        }
        workers[i] = invalid_thread;
    }

    result = true;

    cleanup:
    for (size_t i = 0; i < thread_count; i++) {
        if (!pthread_equal(invalid_thread, workers[i])) {
            pthread_kill(workers[i], SIGKILL);
        }
    }

    free(workers);
    free(worker_args);
    return result;
}

void *block_worker(void *args) {
    block_args *block_args = args;
    bitmap *in_bitmap = block_args->in_bitmap;
    bitmap *out_bitmap = block_args->out_bitmap;

    for (size_t i = block_args->pixel_from; i <= block_args->pixel_to; i++) {
        out_bitmap->data[i] = in_bitmap->max_pixel_val - in_bitmap->data[i];
    }

    return NULL;
}

void *numbers_worker(void *args) {
    numbers_args *numbers_args = args;
    bitmap *in_bitmap = numbers_args->in_bitmap;
    bitmap *out_bitmap = numbers_args->out_bitmap;

    size_t pixel_count = in_bitmap->w * in_bitmap->h;

    for (size_t i = 0; i < pixel_count; i++) {
        if (in_bitmap->data[i] >= numbers_args->color_from && in_bitmap->data[i] <= numbers_args->color_to) { //if body
            out_bitmap->data[i] = in_bitmap->max_pixel_val - in_bitmap->data[i]; //new_value
        }
    }

    return NULL;
}

char *read_in(FILE *in) {
    fseek(in, 0, SEEK_END);
    long file_length = ftell(in);

    char *content = malloc((file_length + 1) * sizeof(*content));
    rewind(in);
    fread(content, sizeof(*content), file_length / sizeof(*content), in);
    content[file_length] = '\0';

    //erase comments
    char *curr = content;
    char *end = &content[file_length];
    while ((curr = strchr(curr, '#'))) {
        char *lf_char = strchr(curr, '\n');
        if (lf_char) {
            memcpy(curr, lf_char, (end - lf_char + 1) * sizeof(*content));
        }
        else {
            *curr = '\0';
        }
    }

    return content;
}

long extract_nanos(struct timespec *stamp) {
    return stamp->tv_sec * 1000000000 + stamp->tv_nsec;
}
