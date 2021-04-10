#include <stdio.h>
#include <signal.h>

void sig_handler(int sig);

int main() {
    struct sigaction act;
    act.sa_handler = sig_handler;
    act.sa_flags = SA_RESETHAND;
    sigemptyset(&act.sa_mask);

    sigaction(SIGUSR1, &act, NULL);
   
    raise(SIGUSR1);
    raise(SIGUSR1);
}

void sig_handler(int sig) {
    printf("SIGUSR1 received\n");
}
