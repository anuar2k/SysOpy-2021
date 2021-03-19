#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>

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

bool square_of_integer(long long n);
FILE_TYPE try_open_read(const char* path);
FILE_TYPE try_open_write(const char* path);
size_t read_line(FILE_TYPE from, char *linebuf);

int main(int argc, char **argv) {
    FILE_TYPE input = try_open_read("dane.txt");
    if (input == FILE_NONE) {
        return EXIT_FAILURE;
    }

    FILE_TYPE output_a = try_open_write("a.txt");
    if (output_a == FILE_NONE) {
        FILE_CLOSE(input);
        return EXIT_FAILURE;
    }

    FILE_TYPE output_b = try_open_write("b.txt");
    if (output_b == FILE_NONE) {
        FILE_CLOSE(input);
        FILE_CLOSE(output_a);
        return EXIT_FAILURE;
    }

    FILE_TYPE output_c = try_open_write("c.txt");
    if (output_c == FILE_NONE) {
        FILE_CLOSE(input);
        FILE_CLOSE(output_b);
        return EXIT_FAILURE;
    }

    size_t even_count = 0;
    size_t n;
    char linebuf[20 + 1 + 1]; //(20 chars positive)/(19 + sign chars negative) + newline + nullchar;

    while ((n = read_line(input, linebuf)) > 0) {
        long long read;

        linebuf[n] = '\0';
        sscanf(linebuf, "%lld", &read);

        if (read % 2 == 0) {
            even_count++;
        }

        long long tens = read / 10 % 10;
        if (tens == 0 || tens == 7) {
            FILE_WRITE_CHARS(output_b, linebuf, n);
        }

        if (square_of_integer(read)) {
            FILE_WRITE_CHARS(output_c, linebuf, n);
        }
    }

    char *message = "Liczb parzystych jest ";
    sprintf(linebuf, "%zu", even_count);
    char newline = '\n';

    FILE_WRITE_CHARS(output_a, message, strlen(message));
    FILE_WRITE_CHARS(output_a, linebuf, strlen(linebuf));
    FILE_WRITE_CHARS(output_a, &newline, 1);

    FILE_CLOSE(input);
    FILE_CLOSE(output_a);
    FILE_CLOSE(output_b);
    FILE_CLOSE(output_c);
}

bool square_of_integer(long long n) {
    if (n < 0) {
        return false;
    }

    long long root = sqrt(n);
    return root * root == n;
}

FILE_TYPE try_open_read(const char* path) {
    FILE_TYPE file = FILE_OPEN_READ(path);
    if (file == FILE_NONE) {
        printf("%s: %s\n", path, strerror(errno));
    }

    return file;
}

FILE_TYPE try_open_write(const char* path) {
    FILE_TYPE file = FILE_OPEN_WRITE(path);
    if (file == FILE_NONE) {
        printf("%s: %s\n", path, strerror(errno));
    }

    return file;
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
