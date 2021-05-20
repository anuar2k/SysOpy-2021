#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#define SANTA_SHIPPED_MAX 3
#define REINDEERS 9
#define ELVES 10
#define ELVES_TRIGGER 3

#define TOTAL_WORKERS (1 + REINDEERS + ELVES)

#define lock(mutex)                                \
for (bool _iter = true;                            \
     _iter && (pthread_mutex_lock(mutex), true);   \
     _iter = (pthread_mutex_unlock(mutex), false))

pthread_mutex_t rand_mut = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t santa_warehouse_mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t santa_warehouse_cond = PTHREAD_COND_INITIALIZER;
size_t idle_reindeers = 0;
size_t stuck_elves = 0;
size_t stuck_elves_ids[ELVES_TRIGGER];

void mutex_cleanup_routine(void *arg);
int rand_sleep(time_t secs);

void *santa_claus(void *id);
void *reindeer(void *id);
void *elf(void *id);

int main(void) {
    int result = EXIT_FAILURE;

    pthread_t invalid_thread = pthread_self();
    pthread_t workers[TOTAL_WORKERS];

    for (size_t i = 0; i < TOTAL_WORKERS; i++) {
        workers[i] = invalid_thread;
    }

    if (pthread_create(&workers[0], NULL, santa_claus, (void*)0) != 0) {
        workers[0] = invalid_thread;
        goto cleanup;
    }

    for (size_t i = 1; i < 1 + REINDEERS; i++) {
        if (pthread_create(&workers[i], NULL, reindeer, (void*)i) != 0) {
            workers[i] = invalid_thread;
            goto cleanup;
        }
    }

    for (size_t i = 1 + REINDEERS; i < 1 + REINDEERS + ELVES; i++) {
        if (pthread_create(&workers[i], NULL, elf, (void*)i) != 0) {
            workers[i] = invalid_thread;
            goto cleanup;
        }
    }

    //wait for Santa to finish and kill other threads
    pthread_join(workers[0], NULL);
    workers[0] = invalid_thread;

    result = EXIT_SUCCESS;

    cleanup:
    for (size_t i = 0; i < TOTAL_WORKERS; i++) {
        if (!pthread_equal(invalid_thread, workers[i])) {
            pthread_cancel(workers[i]);
        }
    }

    for (size_t i = 0; i < TOTAL_WORKERS; i++) {
        if (!pthread_equal(invalid_thread, workers[i])) {
            pthread_join(workers[i], NULL);
        }
    }

    return result;
}

void *santa_claus(void *id) {
    size_t shipped_gifts = 0;

    while (shipped_gifts < SANTA_SHIPPED_MAX) {
        lock (&santa_warehouse_mut) {
            pthread_cleanup_push(mutex_cleanup_routine, &santa_warehouse_mut);

            while (idle_reindeers != REINDEERS && stuck_elves != ELVES_TRIGGER) {
                pthread_cond_wait(&santa_warehouse_cond, &santa_warehouse_mut);
            }

            printf("Mikołaj: budzę się\n");

            if (idle_reindeers == REINDEERS) {
                printf("Mikołaj: dostarczam zabawki\n");
                rand_sleep(2);
                printf("Mikołaj: dostarczyłem zabawki\n");

                idle_reindeers = 0;
                shipped_gifts++;
                pthread_cond_broadcast(&santa_warehouse_cond);
            }
            else {
                printf(
                    "Mikołaj: rozwiązuję problemy elfów %zu, %zu, %zu\n", 
                    stuck_elves_ids[0], 
                    stuck_elves_ids[1], 
                    stuck_elves_ids[2]
                );
                rand_sleep(2);

                stuck_elves = 0;
                pthread_cond_broadcast(&santa_warehouse_cond);
            }

            pthread_cleanup_pop(false);
        }
    }

    return NULL;
}

void *reindeer(void *id) {
    size_t worker_id = (size_t)id;

    while (true) {
        lock (&santa_warehouse_mut) {
            pthread_cleanup_push(mutex_cleanup_routine, &santa_warehouse_mut);

            while (idle_reindeers > 0) {
                pthread_cond_wait(&santa_warehouse_cond, &santa_warehouse_mut);
            }

            pthread_cleanup_pop(false);
        }

        printf("Renifer: dostarczam zabawki, %zu\n", worker_id);
        rand_sleep(2);
        printf("Renifer: lecę na wakacje, %zu\n", worker_id);
        rand_sleep(5);

        lock (&santa_warehouse_mut) {
            pthread_cleanup_push(mutex_cleanup_routine, &santa_warehouse_mut);

            idle_reindeers++;
            printf("Renifer: czeka %zu reniferów na Mikołaja, %zu\n", idle_reindeers, worker_id);

            if (idle_reindeers == REINDEERS) {
                pthread_cond_broadcast(&santa_warehouse_cond);
            }

            pthread_cleanup_pop(false);
        }
    }

    return NULL;
}

void *elf(void *id) {
    size_t worker_id = (size_t)id;

    return NULL;

    while (true) {
        lock (&santa_warehouse_mut) {
            pthread_cleanup_push(mutex_cleanup_routine, &santa_warehouse_mut);
            pthread_cleanup_pop(false);
        }
    }

    return NULL;
}

void mutex_cleanup_routine(void *arg) {
    pthread_mutex_t *mut = arg;
    pthread_mutex_unlock(mut);
}

// sleeps for about [secs; secs + 1] seconds
int rand_sleep(time_t secs) {
    int random;
    lock (&rand_mut) {
        random = rand();
    }

    struct timespec sleep_time = {
        .tv_sec = secs,
        .tv_nsec = 1000 * (random % 1000000)
    };

    return nanosleep(&sleep_time, NULL);
}
