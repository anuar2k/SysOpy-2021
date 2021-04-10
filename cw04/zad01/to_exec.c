#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#ifndef TEST_SIGNAL
    #define TEST_SIGNAL SIGUSR1
#endif

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid argument count\n");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "ignore") == 0) {
        printf("Exec raise() retval: %d\n", raise(TEST_SIGNAL));
    }
    else if (strcmp(argv[1], "mask") == 0) {
        printf("Exec raise() retval: %d\n", raise(TEST_SIGNAL));
    }
    else if (strcmp(argv[1], "pending") == 0) {
        sigset_t pending;
        sigpending(&pending);

        if (sigismember(&pending, TEST_SIGNAL)) {
            printf("Exec - signal is pending\n");
        }
        else {
            printf("Exec - signal is NOT pending\n");
        }
    }
    else {
        fprintf(stderr, "invalid argument\n");
        return EXIT_FAILURE;
    }
}