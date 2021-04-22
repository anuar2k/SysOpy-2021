#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "invalid argument count\n");
        return EXIT_FAILURE;
    }

    short chars_per_read;
    if (sscanf(argv[3], "%hu", &chars_per_read) != 1) {
        fprintf(stderr, "malformed arguments\n");
        return EXIT_FAILURE;
    }

    FILE *source = fopen(argv[1], "r");
    if (!source) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    FILE *target = fopen(argv[2], "w+");
    if (!target) {
        perror(argv[2]);
        return EXIT_FAILURE;
    }

    setbuf(source, NULL);
    setbuf(target, NULL);

    short line_no;
    size_t total_buf_size = sizeof(line_no) + chars_per_read;
    char *buf = malloc(total_buf_size);

    while (fread(buf, 1, total_buf_size, source) == total_buf_size) {
        line_no = *(short*)buf;
        char *payload = buf + sizeof(line_no);

        flock(fileno(target), LOCK_EX);

        rewind(target);

        //find line_no-th newline, and add some if they're missing
        int in;
        while (line_no >= 0) {
            in = fgetc(target);
            if (in == EOF) {
                break;
            }
            if (in == '\n') {
                line_no--;
            }
        }

        while (line_no-- >= 0) {
            fputc('\n', target);
        }

        //move before the searched \n and measure how many bytes are left in the file
        fseek(target, -1, SEEK_CUR);
        long to_move_begin = ftell(target);
        fseek(target, 0, SEEK_END);
        long to_move = ftell(target) - to_move_begin;

        //copy the rest before the searched \n to memory
        char *to_move_buf = malloc(to_move);

        fseek(target, to_move_begin, SEEK_SET);
        fread(to_move_buf, 1, to_move, target);

        //insert payload
        fseek(target, to_move_begin, SEEK_SET);
        fwrite(payload, 1, chars_per_read, target);
        
        //insert the rest
        fwrite(to_move_buf, 1, to_move, target);

        flock(fileno(target), LOCK_UN);
        free(to_move_buf);
    }

    free(buf);
    fclose(source);
    fclose(target);
}
