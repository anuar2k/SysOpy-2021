#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef LIB
    #define FILE_TYPE FILE*
    #define FILE_NONE NULL
    #define FILE_STDOUT stdout
    #define FILE_OPEN_READ(path) fopen(path, "r")
    #define FILE_READ_CHARS(file, buf, len) fread(buf, sizeof(char), len, file)
    #define FILE_WRITE_CHARS(file, buf, len) fwrite(buf, sizeof(char), len, file)
    #define FILE_CLOSE(file) fclose(file)
#else
    #define FILE_TYPE int
    #define FILE_NONE -1
    #define FILE_STDOUT STDOUT_FILENO
    #define FILE_OPEN_READ(path) open(path, O_RDONLY)
    #define FILE_READ_CHARS(file, buf, len) read(file, buf, len * sizeof(char))
    #define FILE_WRITE_CHARS(file, buf, len) write(file, buf, len * sizeof(char))
    #define FILE_CLOSE(file) close(file)
#endif

FILE_TYPE open_from_stdin(const char *message);
size_t rewrite_line(FILE_TYPE from, FILE_TYPE to);

int main(int argc, char **argv) {
    if (argc > 3) {
        printf("invalid argument count\n");
        return EXIT_FAILURE;
    }

    FILE_TYPE file_a = argc >= 2 ? FILE_OPEN_READ(argv[1]) : open_from_stdin("enter path a: ");
    if (file_a == FILE_NONE) {
        printf("%s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    FILE_TYPE file_b = argc >= 3 ? FILE_OPEN_READ(argv[2]) : open_from_stdin("enter path b: ");
    if (file_b == FILE_NONE) {
        printf("%s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    size_t rewritten_a;
    size_t rewritten_b;

    do {
        rewritten_a = rewrite_line(file_a, FILE_STDOUT);
        rewritten_b = rewrite_line(file_b, FILE_STDOUT);
    }
    while (rewritten_a != 0 || rewritten_b != 0);

    FILE_CLOSE(file_a);
    FILE_CLOSE(file_b);
}

FILE_TYPE open_from_stdin(const char *message) {
    fputs(message, stdout);
    char *path = NULL;
    FILE_TYPE file = FILE_NONE;

    if (scanf(" %ms", &path) == 1) {
        file = FILE_OPEN_READ(path);
    }

    free(path);

    return file;
}

size_t rewrite_line(FILE_TYPE from, FILE_TYPE to) {
    size_t rewritten = 0;
    char buf;

    while (FILE_READ_CHARS(from, &buf, 1) == 1) {
        rewritten++;
        FILE_WRITE_CHARS(to, &buf, 1);

        if (buf == '\n') {
            break;
        }
    }

    return rewritten;
}
