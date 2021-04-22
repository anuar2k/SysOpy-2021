#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

void term_handler(int sig) {
    kill(0, SIGTERM);
    _exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "invalid argument count\n");
        return EXIT_FAILURE;
    }

    size_t consumer_count;
    size_t chars_per_read;
    if (sscanf(argv[1], "%zu", &consumer_count) != 1 || sscanf(argv[2], "%zu", &chars_per_read) != 1) {
        fprintf(stderr, "malformed arguments\n");
        return EXIT_FAILURE;
    }

    if (consumer_count < 1 || chars_per_read < 1) {
        fprintf(stderr, "consumer count and N must be positive\n");
        return EXIT_FAILURE;
    }

    //create a group so we can easily terminate all children in case any of them fails
    setpgid(0, 0);

    //create a handler for SIGTERM, so we can shut down all children together with parent process
    struct sigaction act;
    act.sa_handler = term_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    
    sigaction(SIGTERM, &act, NULL);

    const char *fifo_name = tmpnam(NULL);
    if (!fifo_name) {
        fprintf(stderr, "can't open fifo\n");
        return EXIT_FAILURE;
    }

    char out_name[] = "/tmp/runner_outXXXXXX";
    int out_file_fd = mkstemp(out_name);

    if (out_file_fd == -1) {
        perror("out");
        return EXIT_FAILURE;
    }

    FILE *out_file = fdopen(out_file_fd, "r");
    rewind(out_file);

    if (mkfifo(fifo_name, 0666) != 0) {
        perror("fifo");
        return EXIT_FAILURE;
    }

    char *chars_per_read_str;
    asprintf(&chars_per_read_str, "%zu", chars_per_read);

    for (size_t i = 0; i < consumer_count; i++) {
        if (fork() == 0) {
            execl("./consumer", "./consumer", fifo_name, out_name, chars_per_read_str, NULL);
            return EXIT_FAILURE;
        }
    }

    for (size_t i = 3; i < argc; i++) {
        char *line_no_str;
        asprintf(&line_no_str, "%zu", i - 3);
        if (fork() == 0) {
            execl("./producer", "./producer", fifo_name, line_no_str, argv[i], chars_per_read_str, NULL);
            return EXIT_FAILURE;
        }
        free(line_no_str);
    }

    free(chars_per_read_str);

    int wstatus;
    while (wait(&wstatus) > 0) {
        int exit_status;
        if (WIFEXITED(wstatus) && (exit_status = WEXITSTATUS(wstatus)) != 0) {
            fprintf(stderr, "MISSION ABORT, SOME PROCESS RETURNED NONZERO CODE: %d\n", exit_status);
            raise(SIGTERM);
        }
    }

    char *lineptr = NULL;
    size_t n = 0;
    for (size_t i = 3; i < argc; i++) {
        if (getline(&lineptr, &n, out_file) > 0) {
            char *newline = strchr(lineptr, '\n');
            if (!newline) {
                fprintf(stderr, "expected a newline\n");
                return EXIT_FAILURE;
            }
            *newline = '\0';

            FILE *orig_line = fopen(argv[i], "r");
            if (!orig_line) {
                perror(argv[i]);
                return EXIT_FAILURE;
            }

            fseek(orig_line, 0, SEEK_END);
            size_t file_len = ftell(orig_line);
            rewind(orig_line);

            char *orig_line_buf = malloc(file_len + 1);
            orig_line_buf[file_len] = '\0';

            fread(orig_line_buf, 1, file_len, orig_line);

            printf("%s vs %s\n", lineptr, orig_line_buf);

            if (strcmp(orig_line_buf, lineptr) != 0) {
                fprintf(stderr, "lines are not equal\n");
                return EXIT_FAILURE;
            }

            free(orig_line_buf);
            fclose(orig_line);
        }
        else {
            fprintf(stderr, "can't read next line\n");
            return EXIT_FAILURE;
        }
    }

    free(lineptr);
    fclose(out_file);

    printf("files match\n");
}
