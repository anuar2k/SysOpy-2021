#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>

#define TRY_POST(sem_ptr)                \
do {                                     \
    if (sem_post(sem_ptr) == -1) {       \
        perror("sem " #sem_ptr " post"); \
        goto catch;                      \
    }                                    \
}                                        \
while (false)

#define TRY_WAIT(sem_ptr)                \
do {                                     \
    if (sem_wait(sem_ptr) == -1) {       \
        perror("sem " #sem_ptr " wait"); \
        goto catch;                      \
    }                                    \
}                                        \
while (false)

#define TRY_SLEEP(secs)           \
do {                              \
    if (rand_sleep(secs) == -1) { \
        perror("rand_sleep");     \
        goto catch;               \
    }                             \
}                                 \
while (false)

#define OVEN_CAP 5
#define TABLE_CAP 5

typedef short pizza;

typedef struct {
    pizza oven[OVEN_CAP];
    size_t in_oven;
    pizza table[TABLE_CAP];
    size_t on_table;
    sem_t oven_door;
    sem_t oven_free;
    sem_t table_door;
    sem_t table_in_free;
    sem_t table_out_avail;
} pizzeria_shm;

pizzeria_shm *pizzeria = (pizzeria_shm *) -1;

void pizzaiolo(void);
void delivery_guy(void);
void init_signals(void);
int rand_sleep(time_t secs);
long long timestamp(void);
void sig_noop(int sig);
unsigned long mix(unsigned long a, unsigned long b, unsigned long c);

int main(int argc, char **argv) {
    int result = EXIT_FAILURE;

    if (argc != 3) {
        fprintf(stderr, "invalid argument count\n");
        return result;
    }

    size_t pizzaiolos;
    size_t delivery_guys;

    if (sscanf(argv[1], "%zu", &pizzaiolos) != 1 || sscanf(argv[2], "%zu", &delivery_guys) != 1) {
        fprintf(stderr, "malformed arguments\n");
        return result;
    }

    if (pizzaiolos < 1 || delivery_guys < 1) {
        fprintf(stderr, "both arguments must be positive\n");
        return result;
    }

    pizzeria = mmap(NULL, sizeof(*pizzeria), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (pizzeria == (pizzeria_shm *) -1) {
        perror("shmat");
        goto cleanup;
    }

    for (size_t i = 0; i < OVEN_CAP; i++) {
        pizzeria->oven[i] = -1;
    }

    for (size_t i = 0; i < TABLE_CAP; i++) {
        pizzeria->table[i] = -1;
    }

    sem_init(&pizzeria->oven_door,       true, 1);
    sem_init(&pizzeria->oven_free,       true, OVEN_CAP);
    sem_init(&pizzeria->table_door,      true, 1);
    sem_init(&pizzeria->table_in_free,   true, TABLE_CAP);
    sem_init(&pizzeria->table_out_avail, true, 0);

    init_signals();

    while (pizzaiolos-- > 0) {
        if (fork() == 0) {
            /**
             * time(NULL) is not sufficient for a seed, because we run multiple
             * forks in a millisecond, mixing clock(), time(NULL) and getpid()
             * gives good results even for processes spawned immediately
             */
            srand(mix(clock(), time(NULL), getpid()));
            pizzaiolo();
            return EXIT_FAILURE;
        }
    }

    while (delivery_guys-- > 0) {
        if (fork() == 0) {
            srand(mix(clock(), time(NULL), getpid()));
            delivery_guy();
            return EXIT_FAILURE;
        }
    }

    //wait till all children are dead and continue loop on signal (ignore EINTR)
    while (wait(NULL) > 0 || errno == EINTR)
        ;

    result = EXIT_SUCCESS;

    cleanup:
    if (pizzeria != (pizzeria_shm *) -1) {
        sem_destroy(&pizzeria->oven_door);
        sem_destroy(&pizzeria->oven_free);
        sem_destroy(&pizzeria->table_door);
        sem_destroy(&pizzeria->table_in_free);
        sem_destroy(&pizzeria->table_out_avail);
        munmap(pizzeria, sizeof(*pizzeria));
    }

    return result;
}

void pizzaiolo(void) {
    while (true) {
        pizza to_prepare = rand() % 10;

        printf("(%d %lld) przygotowuje pizze: %hd\n", getpid(), timestamp(), to_prepare);

        TRY_SLEEP(1);
        TRY_WAIT(&pizzeria->oven_free);
        TRY_WAIT(&pizzeria->oven_door);

        ssize_t free_spot = -1;
        for (size_t i = 0; i < OVEN_CAP; i++) {
            if (pizzeria->oven[i] == -1) {
                free_spot = i;
                break;
            }
        }

        if (free_spot == -1) {
            fprintf(stderr, "no free spot in oven\n");
            goto catch;
        }

        pizzeria->oven[free_spot] = to_prepare;
        pizzeria->in_oven++;

        printf(
            "(%d %lld) dodalem pizze: %hd; liczba pizz w piecu: %zu\n", 
            getpid(), 
            timestamp(), 
            to_prepare, 
            pizzeria->in_oven
        );

        TRY_POST(&pizzeria->oven_door);
        TRY_SLEEP(4);
        TRY_WAIT(&pizzeria->oven_door);

        pizza removed = pizzeria->oven[free_spot];
        pizzeria->oven[free_spot] = -1;
        pizzeria->in_oven--;

        if (removed != to_prepare) {
            fprintf(stderr, "wrong pizza removed\n");
            goto catch;
        }

        printf(
            "(%d %lld) wyjalem pizze: %hd; liczba pizz w piecu: %zu\n", 
            getpid(), 
            timestamp(), 
            removed, 
            pizzeria->in_oven
        );

        TRY_POST(&pizzeria->oven_door);
        TRY_POST(&pizzeria->oven_free);
        TRY_WAIT(&pizzeria->table_in_free);
        TRY_WAIT(&pizzeria->table_door);

        free_spot = -1;
        for (size_t i = 0; i < TABLE_CAP; i++) {
            if (pizzeria->table[i] == -1) {
                free_spot = i;
                break;
            }
        }

        if (free_spot == -1) {
            fprintf(stderr, "no free spot on table\n");
            goto catch;
        }

        pizzeria->table[free_spot] = removed;
        pizzeria->on_table++;

        printf(
            "(%d %lld) klade na stol pizze: %hd; liczba pizz na stole: %zu\n", 
            getpid(), 
            timestamp(), 
            removed, 
            pizzeria->on_table
        );

        TRY_POST(&pizzeria->table_out_avail);
        TRY_POST(&pizzeria->table_door);
    }

    catch:
    munmap(pizzeria, sizeof(*pizzeria));
}

void delivery_guy(void) {
    while (true) {
        TRY_WAIT(&pizzeria->table_out_avail);
        TRY_WAIT(&pizzeria->table_door);

        ssize_t busy_slot = -1;
        for (size_t i = 0; i < TABLE_CAP; i++) {
            if (pizzeria->table[i] != -1) {
                busy_slot = i;
                break;
            }
        }

        if (busy_slot == -1) {
            fprintf(stderr, "no pizza on table\n");
            goto catch;
        }

        pizza to_deliver = pizzeria->table[busy_slot];
        pizzeria->table[busy_slot] = -1;
        pizzeria->on_table--;

        printf(
            "(%d %lld) pobieram pizze: %hd; liczba pizz na stole: %zu\n", 
            getpid(), 
            timestamp(), 
            to_deliver, 
            pizzeria->on_table
        );

        TRY_POST(&pizzeria->table_in_free);
        TRY_POST(&pizzeria->table_door);
        TRY_SLEEP(4);

        printf(
            "(%d %lld) dostarczam pizze: %hd\n", 
            getpid(), 
            timestamp(), 
            to_deliver
        );

        TRY_SLEEP(4);
    }

    catch:
    munmap(pizzeria, sizeof(*pizzeria));
}

void init_signals(void) {
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGINT);
    sigdelset(&mask, SIGTERM);
    sigdelset(&mask, SIGQUIT);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    struct sigaction act;
    act.sa_handler = sig_noop;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
}

// sleeps for about [secs; secs + 1] seconds
int rand_sleep(time_t secs) {
    struct timespec sleep_time = {
        .tv_sec = secs,
        .tv_nsec = 1000 * (rand() % 1000000)
    };

    return nanosleep(&sleep_time, NULL);
}

long long timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000LL + tv.tv_usec / 1000LL;
}

void sig_noop(int sig) {
    /**
     * no-op to prevent OS from killing the process
     * we want to make blocking calls of IPC to return -1/EINTR
     * so we can clean up the semaphore array and shared memory
     */
}

// http://burtleburtle.net/bob/hash/doobs.html
unsigned long mix(unsigned long a, unsigned long b, unsigned long c)
{
    a=a-b;  a=a-c;  a=a^(c >> 13);
    b=b-c;  b=b-a;  b=b^(a << 8);
    c=c-a;  c=c-b;  c=c^(b >> 13);
    a=a-b;  a=a-c;  a=a^(c >> 12);
    b=b-c;  b=b-a;  b=b^(a << 16);
    c=c-a;  c=c-b;  c=c^(b >> 5);
    a=a-b;  a=a-c;  a=a^(c >> 3);
    b=b-c;  b=b-a;  b=b^(a << 10);
    c=c-a;  c=c-b;  c=c^(b >> 15);
    return c;
}
