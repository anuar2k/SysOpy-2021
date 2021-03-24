#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/times.h>

#include <rrmerge.h>

int main(int argc, char **argv) {
    struct tms tms_measure_start;
    clock_t real_measure_start = times(&tms_measure_start);

    for (int i = 1; i < argc; i++) {
        if (strchr(argv[i], ':') == NULL) {
            fprintf(stderr, "argument %d is malformed\n", i - 1);
            return EXIT_FAILURE;
        }
    }

    //init structures once, work on their copies in child processes
    v_v_char row_blocks;
    v_file_pair file_pairs;
    v_FILE tmp_files;

    vec_init(&row_blocks);
    vec_init(&file_pairs);
    vec_init(&tmp_files);

    for (int i = 1; i < argc; i++) {
        if (fork() == 0) {
            add_file_pair(&file_pairs, argv[i]);
            merge_file_pairs(&tmp_files, &file_pairs);
            add_row_block(&row_blocks, tmp_files.storage[0]);

            free_row_blocks(&row_blocks);
            free_file_pairs(&file_pairs);
            free_tmp_files(&tmp_files);
            return EXIT_SUCCESS;
        }
    }
    
    //consume zombies, wait for all children to finish
    while (wait(NULL) > 0)
        ;

    //formal calls in case these vectors were preinitialized (they are not)
    ///in fact these structures were filled with data only in child processes
    free_row_blocks(&row_blocks);
    free_file_pairs(&file_pairs);
    free_tmp_files(&tmp_files);

    struct tms tms_measure_end;
    clock_t real_measure_end = times(&tms_measure_end);

    clock_t real_elapsed = real_measure_end - real_measure_start;
    clock_t user_elapsed = tms_measure_end.tms_utime - tms_measure_start.tms_utime;
    clock_t system_elapsed = tms_measure_end.tms_stime - tms_measure_start.tms_stime;

    printf("measured time: real, user, sys\n");
    printf("%ld %ld %ld\n", real_elapsed, user_elapsed, system_elapsed);
}
