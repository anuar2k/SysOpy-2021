#include <stdio.h>
#include <signal.h>

volatile sig_atomic_t recursive_sig1 = 0;
volatile sig_atomic_t recursive_sig2 = 0;

void sig1_handler(int sig);
void sig2_handler(int sig);

int main() {
    {
        struct sigaction act;
        act.sa_handler = sig1_handler;
        act.sa_flags = 0;
        sigemptyset(&act.sa_mask);

        sigaction(SIGUSR1, &act, NULL);
    }
    {
        struct sigaction act;
        act.sa_handler = sig2_handler;
        act.sa_flags = SA_NODEFER;
        sigemptyset(&act.sa_mask);

        sigaction(SIGUSR2, &act, NULL);
    }

    raise(SIGUSR1);
    raise(SIGUSR2);
}

void sig1_handler(int sig) {
    if (recursive_sig1 == 0) {
        recursive_sig1 = 1;
        raise(SIGUSR1);
    }
    else if (recursive_sig1 == 1) {
        printf("SIGUSR1 recursive call\n");
    }
    recursive_sig1 = 2;
}

void sig2_handler(int sig) {
    if (recursive_sig2 == 0) {
        recursive_sig2 = 1;
        raise(SIGUSR2);
    }
    else if (recursive_sig2 == 1) {
        printf("SIGUSR2 recursive call\n");
    }
    recursive_sig2 = 2;
}
