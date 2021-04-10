#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define TYPE_KILL 0
#define TYPE_SIGQUEUE 1
#define TYPE_SIGRT 2

volatile sig_atomic_t sig1_received = 0;
volatile sig_atomic_t sig2_received = 0;
volatile sig_atomic_t sender_type;
volatile pid_t sender_pid;

void sig_handler(int sig, siginfo_t *info, void *ctx);

int main() {
    printf("PID: %d\n", getpid());

    sigset_t block_all;
    sigfillset(&block_all);
    sigdelset(&block_all, SIGINT);
    sigprocmask(SIG_SETMASK, &block_all, NULL);

    sigset_t unblock;
    sigfillset(&unblock);
    sigdelset(&unblock, SIGINT);
    sigdelset(&unblock, SIGUSR1);
    sigdelset(&unblock, SIGUSR2);
    sigdelset(&unblock, SIGRTMIN+0);
    sigdelset(&unblock, SIGRTMIN+1);

    struct sigaction act;
    act.sa_sigaction = sig_handler;
    act.sa_flags = SA_SIGINFO;
    sigemptyset(&act.sa_mask);

    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGUSR2, &act, NULL);
    sigaction(SIGRTMIN+0, &act, NULL);
    sigaction(SIGRTMIN+1, &act, NULL);

    while (!sig2_received) {
        sigsuspend(&unblock);
    }

    switch (sender_type) {
        case TYPE_KILL: {
            for (size_t i = 0; i < sig1_received; i++) {
                if (kill(sender_pid, SIGUSR1) == -1) {
                    perror(NULL);
                    return EXIT_FAILURE;
                }
            }

            if (kill(sender_pid, SIGUSR2) == -1) {
                perror(NULL);
                return EXIT_FAILURE;
            }

            printf("received %d SIGUSR1 signals\n", sig1_received);
            break;
        }
        case TYPE_SIGQUEUE: {
            for (size_t i = 0; i < sig1_received; i++) {
                if (sigqueue(sender_pid, SIGUSR1, (union sigval){ 0 }) == -1) {
                    perror(NULL);
                    return EXIT_FAILURE;
                }
            }

            union sigval data = {
                .sival_int = sig1_received
            };

            if (sigqueue(sender_pid, SIGUSR2, data) == -1) {
                perror(NULL);
                return EXIT_FAILURE;
            }

            printf("received %d SIGUSR1 signals\n", sig1_received);
            break;
        }
        case TYPE_SIGRT: {
            for (size_t i = 0; i < sig1_received; i++) {
                if (kill(sender_pid, SIGRTMIN+0) == -1) {
                    perror(NULL);
                    return EXIT_FAILURE;
                }
            }

            if (kill(sender_pid, SIGRTMIN+1) == -1) {
                perror(NULL);
                return EXIT_FAILURE;
            }

            printf("received %d SIGRTMIN+0 signals\n", sig1_received);
            break;
        }
    }
}

void sig_handler(int sig, siginfo_t *info, void *ctx) {
    if (sig == SIGUSR1 || sig == SIGRTMIN+0) {
        sig1_received++;
    }
    else if (sig == SIGUSR2 || sig == SIGRTMIN+1) {
        sig2_received++;
        sender_pid = info->si_pid;

        if (info->si_code == SI_USER) {
            sender_type = sig == SIGRTMIN+1 ? TYPE_SIGRT : TYPE_KILL;
        }
        else if (info->si_code == SI_QUEUE) {
            sender_type = TYPE_SIGQUEUE;
        }
    }
}
