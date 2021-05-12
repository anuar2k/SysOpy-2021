#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/times.h>

#define S(x) #x

typedef struct {
    size_t w;
    size_t h;
    uint8_t *data;
} bitmap;

const char *WHITESPACE_DELIM = " \f\n\r\t\v"; //see man isspace
struct tms tms_ignore;

bool parse_in(FILE *in, bitmap *in_bitmap);
bool write_out(FILE *out, bitmap *out_bitmap);
bool process_block(size_t thread_count, bitmap *in_bitmap, bitmap *out_bitmap);
bool process_numbers(size_t thread_count, bitmap *in_bitmap, bitmap *out_bitmap);
char *read_in(FILE *in);

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
    if (sscanf(argv[1], "%zu", &thread_count) != 1) {
        perror(S(thread_count));
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

    clock_t start_time = times(&tms_ignore);

    if (block) {
        if (!process_block(thread_count, &in_bitmap, &out_bitmap)) {
            fprintf(stderr, S(process_block) " fail\n");
            goto cleanup;
        }
    }
    else {
        if (!process_numbers(thread_count, &in_bitmap, &out_bitmap)) {
            fprintf(stderr, S(process_numbers) " fail\n");
            goto cleanup;
        }
    }

    clock_t result_time = times(&tms_ignore) - start_time;

    if (!write_out(out, &out_bitmap)) {
        fprintf(stderr, S(write_out) " fail\n");
        goto cleanup;
    }

    printf("CLOCKS_PER_SEC: %ld, result time: %ld\n", CLOCKS_PER_SEC, result_time);

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
    if (!curr || strcmp("P2", curr) != 0) goto cleanup;

    curr = strtok(NULL, WHITESPACE_DELIM);
    if (!curr || sscanf(curr, "%zu", &in_bitmap->w) != 1) goto cleanup;
    if (in_bitmap->w < 1) goto cleanup;

    curr = strtok(NULL, WHITESPACE_DELIM);
    if (!curr || sscanf(curr, "%zu", &in_bitmap->h) != 1) goto cleanup;
    if (in_bitmap->w < 1) goto cleanup;

    size_t max_val;
    curr = strtok(NULL, WHITESPACE_DELIM);
    if (!curr || sscanf(curr, "%zu", &max_val) != 1) goto cleanup;
    if (max_val != 255) goto cleanup;

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
    fprintf(out, "P2\n%zu %zu\n255\n", out_bitmap->w, out_bitmap->h);

    size_t i = 0;
    for (size_t h = 0; h < out_bitmap->h; h++) {
        for (size_t w = 0; w < out_bitmap->w; w++) {
            fprintf(out, w == 0 ? "%hhd" : " %hhd", out_bitmap->data[i++]);
        }
        fputc('\n', out);
    }

    return true;
}

bool process_block(size_t thread_count, bitmap *in_bitmap, bitmap *out_bitmap) {
    out_bitmap->w = in_bitmap->w;
    out_bitmap->h = in_bitmap->h;

    size_t pixel_count = out_bitmap->w * out_bitmap->h;
    out_bitmap->data = malloc(pixel_count * sizeof(*out_bitmap->data));
    memcpy(out_bitmap->data, in_bitmap->data, pixel_count * sizeof(*out_bitmap->data));

    return true;
}

bool process_numbers(size_t thread_count, bitmap *in_bitmap, bitmap *out_bitmap) {
    out_bitmap->w = in_bitmap->w;
    out_bitmap->h = in_bitmap->h;

    size_t pixel_count = out_bitmap->w * out_bitmap->h;
    out_bitmap->data = malloc(pixel_count * sizeof(*out_bitmap->data));
    memcpy(out_bitmap->data, in_bitmap->data, pixel_count * sizeof(*out_bitmap->data));
    
    return true;
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
