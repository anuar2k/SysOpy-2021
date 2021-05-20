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
pthread_cond_t santa_wakeup_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t reindeers_wakeup_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t elves_wakeup_cond = PTHREAD_COND_INITIALIZER;

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
                pthread_cond_wait(&santa_wakeup_cond, &santa_warehouse_mut);
            }

            printf("Mikołaj: budzę się\n");

            if (idle_reindeers == REINDEERS) {
                printf("Mikołaj: dostarczam zabawki\n");
                rand_sleep(2);
                printf("Mikołaj: dostarczyłem zabawki\n");

                idle_reindeers = 0;
                shipped_gifts++;
                pthread_cond_broadcast(&reindeers_wakeup_cond);
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
                pthread_cond_broadcast(&elves_wakeup_cond);
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

            //wait until reindeers are dealt with
            while (idle_reindeers > 0) {
                pthread_cond_wait(&reindeers_wakeup_cond, &santa_warehouse_mut);
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
                pthread_cond_signal(&santa_wakeup_cond);
            }

            pthread_cleanup_pop(false);
        }
    }

    return NULL;
}

void *elf(void *id) {
    size_t worker_id = (size_t)id;

    size_t last_spot = ELVES_TRIGGER + 1;

    while (true) {
        lock (&santa_warehouse_mut) {
            pthread_cleanup_push(mutex_cleanup_routine, &santa_warehouse_mut);

            //wait until Santa Claus is done dealing with me
            while (last_spot < stuck_elves && stuck_elves_ids[last_spot] == worker_id) {
                pthread_cond_wait(&elves_wakeup_cond, &santa_warehouse_mut);
            }

            pthread_cleanup_pop(false);
        }

        printf("Elf: pracuję, %zu\n", worker_id);
        rand_sleep(2);

        lock (&santa_warehouse_mut) {
            pthread_cleanup_push(mutex_cleanup_routine, &santa_warehouse_mut);

            //try to find a spot in stuck_elves_ids
            bool first_try = true;
            while (stuck_elves == ELVES_TRIGGER) {
                if (first_try) {
                    printf("Elf: czeka na powrót elfów, %zu\n", worker_id);
                    first_try = false;
                }

                pthread_cond_wait(&elves_wakeup_cond, &santa_warehouse_mut);
            }

            last_spot = stuck_elves;
            stuck_elves_ids[stuck_elves++] = worker_id;
            printf("Elf: czeka %zu elfów na Mikołaja, %zu\n", stuck_elves, worker_id);

            if (stuck_elves == ELVES_TRIGGER) {
                pthread_cond_signal(&santa_wakeup_cond);
            }

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
