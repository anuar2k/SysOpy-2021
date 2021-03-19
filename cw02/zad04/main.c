#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#ifdef LIB
    #define FILE_TYPE FILE*
    #define FILE_NONE NULL
    #define FILE_OPEN_READ(path) fopen(path, "r")
    #define FILE_OPEN_WRITE(path) fopen(path, "w")
    #define FILE_READ_CHARS(file, buf, len) fread(buf, sizeof(char), len, file)
    #define FILE_WRITE_CHARS(file, buf, len) fwrite(buf, sizeof(char), len, file)
    #define FILE_CLOSE(file) fclose(file)
#else
    #define FILE_TYPE int
    #define FILE_NONE -1
    #define FILE_OPEN_READ(path) open(path, O_RDONLY)
    #define FILE_OPEN_WRITE(path) open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
    #define FILE_READ_CHARS(file, buf, len) read(file, buf, len * sizeof(char))
    #define FILE_WRITE_CHARS(file, buf, len) write(file, buf, len * sizeof(char))
    #define FILE_CLOSE(file) close(file)
#endif

bool kmp_file_replace(char *from_path, char *to_path, char *needle, char *replacement);

int main(int argc, char **argv) {
    if (argc != 5) {
        printf("invalid argument count\n");
        return EXIT_FAILURE;
    }

    if (!kmp_file_replace(argv[1], argv[2], argv[3], argv[4])) {
        return EXIT_FAILURE;
    }
}

bool kmp_file_replace(char *from_path, char *to_path, char *needle, char *replacement) {
    FILE_TYPE from = FILE_OPEN_READ(from_path);
    if (from == FILE_NONE) {
        printf("%s\n", strerror(errno));
        return false;
    }

    FILE_TYPE to = FILE_OPEN_WRITE(to_path);
    if (to == FILE_NONE) {
        printf("%s\n", strerror(errno));
        FILE_CLOSE(from);
        return false;
    }

    size_t needle_len = strlen(needle);
    size_t replacement_len = strlen(replacement);
    if (needle_len == 0) {
        return false;
    }

    size_t tail_len = needle_len + 1;
    char *tail = malloc(tail_len * sizeof(*tail));

    size_t head_idx = 0;
    size_t tail_idx = 0;

    size_t *prefix_fun = malloc(needle_len * sizeof(*prefix_fun));
    size_t k = 0;

    prefix_fun[0] = 0;
    for (size_t q = 1; q < needle_len; q++) {
        while (k > 0 && needle[k] != needle[q]) {
            k = prefix_fun[k - 1];
        }

        if (needle[k] == needle[q]) {
            k++;
        }

        prefix_fun[q] = k;
    }

    size_t matches = 0;

    while (FILE_READ_CHARS(from, &tail[head_idx], 1) == 1) {
        while (matches > 0 && needle[matches] != tail[head_idx]) {
            matches = prefix_fun[matches - 1];
        }

        if (needle[matches] == tail[head_idx]) {
            matches++;
        }

        if (matches == needle_len) {
            matches = 0;

            head_idx = 0;
            tail_idx = 0;
            FILE_WRITE_CHARS(to, replacement, replacement_len);
        }
        else {
            head_idx = (head_idx + 1) % tail_len;

            size_t distance = head_idx >= tail_idx 
                            ? head_idx - tail_idx 
                            : tail_len - tail_idx + head_idx;
            
            if (distance == needle_len) {
                FILE_WRITE_CHARS(to, &tail[tail_idx], 1);
                tail_idx = (tail_idx + 1) % tail_len;
            }
        }
    }

    while (tail_idx != head_idx) {
        FILE_WRITE_CHARS(to, &tail[tail_idx], 1);
        tail_idx = (tail_idx + 1) % tail_len;
    }

    free(tail);
    free(prefix_fun);

    FILE_CLOSE(from);
    FILE_CLOSE(to);

    return true;
}
