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

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("invalid argument count\n");
        return EXIT_FAILURE;
    }

    FILE_TYPE from = FILE_OPEN_READ(argv[1]);
    if (from == FILE_NONE) {
        printf("%s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    FILE_TYPE to = FILE_OPEN_WRITE(argv[2]);
    if (to == FILE_NONE) {
        printf("%s\n", strerror(errno));
        FILE_CLOSE(from);
        return EXIT_FAILURE;
    }

    size_t line_length = 0;
    char newline = '\n';
    char buf;
    while (FILE_READ_CHARS(from, &buf, 1) == 1) {
        FILE_WRITE_CHARS(to, &buf, 1);
        line_length++;

        if (buf == '\n') {
            line_length = 0;
        }
        else {
            if (line_length == 50) {
                FILE_WRITE_CHARS(to, &newline, 1);
                line_length = 0;
            }
        }
    }

    FILE_CLOSE(from);
    FILE_CLOSE(to);
}
