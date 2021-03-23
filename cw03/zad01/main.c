#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid argument count\n");
        return EXIT_FAILURE;
    }
    
    int child_count;
    if (sscanf(argv[1], "%d", &child_count) != 1) {
        fprintf(stderr, "malformed parameter\n");
        return EXIT_FAILURE;
    }
    if (child_count < 1) {
        fprintf(stderr, "child count must be a positive number\n");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < child_count; i++) {
        if (fork() == 0) {
            printf("hello from child %d with pid %d\n", i, getpid());
            return EXIT_SUCCESS;
        }
    }

    while (wait(NULL) > 0) {
        //consume zombies, wait for all children to finish
    }
}
