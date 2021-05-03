#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <unistd.h>

#define TRY_SEM(sem, op)               \
do {                                   \
    struct sembuf sop = {              \
        .sem_num = sem,                \
        .sem_op = op,                  \
        .sem_flg = SEM_UNDO            \
    };                                 \
    if (semop(semid, &sop, 1) == -1) { \
        perror("sem " #sem " " #op);   \
        goto catch;                    \
    }                                  \
}                                      \
while (0)

#define TRY_SLEEP(secs)           \
do {                              \
    if (rand_sleep(secs) == -1) { \
        perror("rand_sleep");     \
        goto catch;               \
    }                             \
}                                 \
while (0)

#define OVEN_CAP 5
#define TABLE_CAP 5         

union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
    struct seminfo  *__buf;
};

typedef short pizza;

typedef enum {
    OVEN_DOOR,
    OVEN_FREE,
    TABLE_DOOR,
    TABLE_IN_FREE,
    TABLE_OUT_AVAIL,
    pizzeria_sem_len
} pizzeria_sem;

typedef struct {
    pizza oven[OVEN_CAP];
    size_t in_oven;
    pizza table[TABLE_CAP];
    size_t on_table;
} pizzeria_shm;

int semid = -1;
int shmid = -1;
pizzeria_shm *pizzeria = (pizzeria_shm *) -1;

void pizzaiolo(void);
void delivery_guy(void);
void init_signals(void);
int rand_sleep(time_t secs);
long long timestamp(void);
void sig_noop(int sig);

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

    semid = semget(IPC_PRIVATE, pizzeria_sem_len, 0666);
    if (semid == -1) {
        perror("semget");
        goto cleanup;
    }

    shmid = shmget(IPC_PRIVATE, sizeof(*pizzeria), 0666);
    if (shmid == -1) {
        perror("shmget");
        goto cleanup;
    }

    pizzeria = shmat(shmid, NULL, 0);
    if (pizzeria == (pizzeria_shm *) -1) {
        perror("shmat");
        goto cleanup;
    }

    semctl(semid, OVEN_DOOR,       SETVAL, (union semun){ .val = 1         });
    semctl(semid, OVEN_FREE,       SETVAL, (union semun){ .val = OVEN_CAP  });
    semctl(semid, TABLE_DOOR,      SETVAL, (union semun){ .val = 1         });
    semctl(semid, TABLE_IN_FREE,   SETVAL, (union semun){ .val = TABLE_CAP });
    semctl(semid, TABLE_OUT_AVAIL, SETVAL, (union semun){ .val = 0         });

    for (size_t i = 0; i < OVEN_CAP; i++) {
        pizzeria->oven[i] = -1;
    }

    for (size_t i = 0; i < TABLE_CAP; i++) {
        pizzeria->table[i] = -1;
    }

    init_signals();

    while (pizzaiolos-- > 0) {
        if (fork() == 0) {
            srand(time(NULL));
            pizzaiolo();
            return EXIT_FAILURE;
        }
    }

    while (delivery_guys-- > 0) {
        if (fork() == 0) {
            srand(time(NULL));
            delivery_guy();
            return EXIT_FAILURE;
        }
    }

    while (wait(NULL) > 0)
        ;

    result = EXIT_SUCCESS;

    cleanup:
    if (pizzeria != (pizzeria_shm *) -1) {
        shmdt(pizzeria);
    }
    if (shmid != -1) {
        shmctl(shmid, IPC_RMID, NULL);
    }
    if (semid != -1) {
        semctl(semid, -1, IPC_RMID);
    }

    return result;
}

void pizzaiolo(void) {
    while (true) {
        pizza to_prepare = rand() % 10;

        printf("(%d %lld) przygotowuje pizze: %hd\n", getpid(), timestamp(), to_prepare);

        TRY_SLEEP(1);
        TRY_SEM(OVEN_FREE, -1);
        TRY_SEM(OVEN_DOOR, -1);

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

        TRY_SEM(OVEN_DOOR, 1);
        TRY_SLEEP(4);
        TRY_SEM(OVEN_DOOR, -1);

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

        TRY_SEM(OVEN_DOOR, 1);
        TRY_SEM(OVEN_FREE, 1);
        TRY_SEM(TABLE_IN_FREE, -1);
        TRY_SEM(TABLE_DOOR, -1);

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

        TRY_SEM(TABLE_OUT_AVAIL, 1);
        TRY_SEM(TABLE_DOOR, 1);
    }

    catch:
    shmdt(pizzeria);
}

void delivery_guy(void) {
    while (true) {
        TRY_SEM(TABLE_OUT_AVAIL, -1);
        TRY_SEM(TABLE_DOOR, -1);

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

        TRY_SEM(TABLE_IN_FREE, 1);
        TRY_SEM(TABLE_DOOR, 1);
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
    shmdt(pizzeria);
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
