#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct timespec rem;

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "invalid argument count\n");
        return EXIT_FAILURE;
    }

    short line_no;
    short chars_per_read;

    if (sscanf(argv[2], "%hu", &line_no) != 1 || sscanf(argv[4], "%hu", &chars_per_read) != 1) {
        fprintf(stderr, "malformed arguments\n");
        return EXIT_FAILURE;
    }

    FILE *source = fopen(argv[3], "r");
    if (!source) {
        perror(argv[3]);
        return EXIT_FAILURE;
    }

    FILE *target = fopen(argv[1], "w");
    if (!target) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    setbuf(source, NULL);
    setbuf(target, NULL);

    size_t total_buf_size = sizeof(line_no) + chars_per_read;
    char *buf = malloc(total_buf_size);
    *(short*)buf = line_no;

    while (fread(buf + sizeof(line_no), 1, chars_per_read, source) == chars_per_read) {
        struct timespec sleep_time = {
            .tv_sec = 1,
            .tv_nsec = 1000 * (rand() % 1000000)
        };

        nanosleep(&sleep_time, &rem);

        if (fwrite(buf, 1, total_buf_size, target) != total_buf_size) {
            perror(argv[1]);
            return EXIT_FAILURE;
        }
    }

    free(buf);
    fclose(source);
    fclose(target);
}
