#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef TEST_SIGNAL
    #define TEST_SIGNAL SIGUSR1
#endif

void sig_handler(int sig);

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid argument count\n");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "ignore") == 0) {
        struct sigaction act;
        act.sa_handler = SIG_IGN;
        act.sa_flags = 0;
        sigemptyset(&act.sa_mask);

        sigaction(TEST_SIGNAL, &act, NULL);
        
        printf("Parent raise() retval: %d\n", raise(TEST_SIGNAL));

        fflush(NULL);
        if (fork() == 0) {
            printf("Child raise() retval: %d\n", raise(TEST_SIGNAL));

            execl("./to_exec", "./to_exec", "ignore", NULL);
        }
    }
    else if (strcmp(argv[1], "handler") == 0) {
        struct sigaction act;
        act.sa_handler = sig_handler;
        act.sa_flags = 0;
        sigemptyset(&act.sa_mask);

        sigaction(TEST_SIGNAL, &act, NULL);

        printf("Parent raise() retval: %d\n", raise(TEST_SIGNAL));

        fflush(NULL);
        if (fork() == 0) {
            printf("Child raise() retval: %d\n", raise(TEST_SIGNAL));
        }
    }
    else if (strcmp(argv[1], "mask") == 0) {
        struct sigaction act;
        act.sa_handler = sig_handler;
        act.sa_flags = 0;
        sigemptyset(&act.sa_mask);

        sigaction(TEST_SIGNAL, &act, NULL);

        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, TEST_SIGNAL);

        sigprocmask(SIG_BLOCK, &mask, NULL);

        printf("Parent raise() retval: %d\n", raise(TEST_SIGNAL));

        fflush(NULL);
        if (fork() == 0) {
            printf("Child raise() retval: %d\n", raise(TEST_SIGNAL));
            fflush(NULL);

            execl("./to_exec", "./to_exec", "mask", NULL);
        }
    }
    else if (strcmp(argv[1], "pending") == 0) {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, TEST_SIGNAL);

        sigprocmask(SIG_BLOCK, &mask, NULL);

        printf("Parent raise() retval: %d\n", raise(TEST_SIGNAL));

        sigset_t pending;
        sigpending(&pending);

        if (sigismember(&pending, TEST_SIGNAL)) {
            printf("Parent - signal is pending\n");
        }
        else {
            printf("Parent - signal is NOT pending\n");
        }

        fflush(NULL);
        if (fork() == 0) {
            sigpending(&pending);

            if (sigismember(&pending, TEST_SIGNAL)) {
                printf("Child - signal is pending\n");
            }
            else {
                printf("Child - signal is NOT pending\n");
            }

            fflush(NULL);

            execl("./to_exec", "./to_exec", "pending", NULL);
        }
    }
    else {
        fprintf(stderr, "invalid argument\n");
        return EXIT_FAILURE;
    }

    wait(NULL);
}

void sig_handler(int sig) {
    printf("PID %d received signal %d\n", getpid(), sig);
}
