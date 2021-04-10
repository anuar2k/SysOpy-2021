#include <stdio.h>
#include <signal.h>
#include <sys/times.h>

void sig_handler(int signum, siginfo_t *info, void *ctx);

int main() {
    struct sigaction act;
    act.sa_sigaction = sig_handler;
    act.sa_flags = SA_SIGINFO;
    sigemptyset(&act.sa_mask);

    sigaction(SIGUSR1, &act, NULL);

    raise(SIGUSR1);
}

void sig_handler(int signum, siginfo_t *info, void *ctx) {
    printf("received signal %d\n", info->si_signo);
    printf("status: %d\n", info->si_status);
    printf("sender PID: %d\n", info->si_pid);
    printf("sender UID: %d\n", info->si_uid);
    printf("utime: %ld\n", info->si_utime);
}
