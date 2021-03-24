#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        perror("invalid argument count\n");
        return EXIT_FAILURE;
    }
    
    size_t child_count;
    if (sscanf(argv[1], "%zu", &child_count) != 1) {
        perror("malformed parameter\n");
        return EXIT_FAILURE;
    }
    if (child_count < 1) {
        perror("child count must be a positive number\n");
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < child_count; i++) {
        if (fork() == 0) {
            printf("hello from child %zu with pid %d\n", i, getpid());
            return EXIT_SUCCESS;
        }
    }

    //consume zombies, wait for all children to finish
    while (wait(NULL) > 0)
        ;
}
