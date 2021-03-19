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

size_t read_line(FILE_TYPE from, char *linebuf);

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("invalid argument count\n");
        return EXIT_FAILURE;
    }

    if (strlen(argv[1]) != 1) {
        printf("first argument must be a character\n");
        return EXIT_FAILURE;
    }

    FILE_TYPE file = FILE_OPEN_READ(argv[2]);
    if (file == FILE_NONE) {
        printf("%s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    char needle = argv[1][0];

    char linebuf[256];
    size_t n;

    while ((n = read_line(file, linebuf)) > 0) {
        if (memchr(linebuf, needle, n)) {
            FILE_WRITE_CHARS(FILE_STDOUT, linebuf, n);
        }
    }

    FILE_CLOSE(file);
}

size_t read_line(FILE_TYPE from, char *linebuf) {
    size_t n = 0;

    while (FILE_READ_CHARS(from, linebuf, 1) == 1) {
        n++;
        if (*linebuf == '\n') {
            break;
        }
        linebuf++;
    }

    return n;
}
