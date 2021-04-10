#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

volatile sig_atomic_t sig1_received = 0;
volatile sig_atomic_t sig2_received = 0;
volatile sig_atomic_t catcher_received;

void sig_handler(int sig, siginfo_t *info, void *ctx);

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "invalid argument count\n");
        return EXIT_FAILURE;
    }

    sigset_t block_all;
    sigfillset(&block_all);
    sigprocmask(SIG_SETMASK, &block_all, NULL);

    sigset_t unblock;
    sigfillset(&unblock);
    sigdelset(&unblock, SIGUSR1);
    sigdelset(&unblock, SIGUSR2);
    sigdelset(&unblock, SIGRTMIN+0);
    sigdelset(&unblock, SIGRTMIN+1);
    
    pid_t catcher_pid;
    size_t sig_count;

    if (sscanf(argv[1], "%d", &catcher_pid) != 1 || sscanf(argv[2], "%zu", &sig_count) != 1) {
        fprintf(stderr, "invalid arguments\n");
        return EXIT_FAILURE;
    }

    struct sigaction act;
    act.sa_sigaction = sig_handler;
    act.sa_flags = SA_SIGINFO;
    sigemptyset(&act.sa_mask);

    if (strcmp(argv[3], "KILL") == 0) {
        sigaction(SIGUSR1, &act, NULL);
        sigaction(SIGUSR2, &act, NULL);

        for (size_t i = 0; i < sig_count; i++) {
            if (kill(catcher_pid, SIGUSR1) == -1) {
                perror(NULL);
                return EXIT_FAILURE;
            }
        }

        if (kill(catcher_pid, SIGUSR2) == -1) {
            perror(NULL);
            return EXIT_FAILURE;
        }

        while (!sig2_received) {
            sigsuspend(&unblock);
        }

        printf("received %d SIGUSR1 signals of %zu sent\n", sig1_received, sig_count);
    }
    else if (strcmp(argv[3], "SIGQUEUE") == 0) {
        sigaction(SIGUSR1, &act, NULL);
        sigaction(SIGUSR2, &act, NULL);

        for (size_t i = 0; i < sig_count; i++) {
            if (sigqueue(catcher_pid, SIGUSR1, (union sigval){ 0 }) == -1) {
                perror(NULL);
                return EXIT_FAILURE;
            }
        }

        if (sigqueue(catcher_pid, SIGUSR2, (union sigval){ 0 }) == -1) {
            perror(NULL);
            return EXIT_FAILURE;
        }

        while (!sig2_received) {
            sigsuspend(&unblock);
        }

        printf(
            "received %d SIGUSR1 signals of %zu sent; catcher received %d\n",
            sig1_received,
            sig_count,
            catcher_received
        );
    }
    else if (strcmp(argv[3], "SIGRT") == 0) {
        sigaction(SIGRTMIN+0, &act, NULL);
        sigaction(SIGRTMIN+1, &act, NULL);

        for (size_t i = 0; i < sig_count; i++) {
            if (kill(catcher_pid, SIGRTMIN+0) == -1) {
                perror(NULL);
                return EXIT_FAILURE;
            }
        }

        if (kill(catcher_pid, SIGRTMIN+1) == -1) {
            perror(NULL);
            return EXIT_FAILURE;
        }

        while (!sig2_received) {
            sigsuspend(&unblock);
        }

        printf("received %d SIGRTMIN+0 signals of %zu sent\n", sig1_received, sig_count);
    }
    else {
        fprintf(stderr, "invalid mode\n");
        return EXIT_FAILURE;
    }
}

void sig_handler(int sig, siginfo_t *info, void *ctx) {
    if (sig == SIGUSR1 || sig == SIGRTMIN+0) {
        sig1_received++;
    }
    else if (sig == SIGUSR2 || sig == SIGRTMIN+1) {
        sig2_received++;
        if (info->si_code == SI_QUEUE) {
            catcher_received = info->si_value.sival_int;
        }
    }
}
