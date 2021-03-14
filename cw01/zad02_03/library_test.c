#include <rrmerge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/times.h>

#ifdef DYNAMIC
    #include <dlfcn.h>
#endif

#define RRMERGE_FUNCTIONS(PROCESS_DECL) \
        PROCESS_DECL(size_t, add_row_block, v_v_char *row_blocks, FILE *merged_file) \
        PROCESS_DECL(void, remove_row_block, v_v_char *row_blocks, size_t block_idx) \
        PROCESS_DECL(void, remove_row, v_v_char *row_blocks, size_t block_idx, size_t row_idx) \
        PROCESS_DECL(void, print_row_blocks, v_v_char* row_blocks) \
        PROCESS_DECL(void, free_row_blocks, v_v_char *row_blocks) \
        \
        PROCESS_DECL(void, add_file_pair, v_file_pair* file_pairs, char *path_pair) \
        PROCESS_DECL(void, merge_file_pairs, v_FILE *tmp_files, v_file_pair *file_pairs) \
        PROCESS_DECL(void, free_file_pairs, v_file_pair *file_pairs) \
        \
        PROCESS_DECL(void, free_tmp_files, v_FILE *tmp_files) \
        \
        PROCESS_DECL(void, vec_init, ptr_vector *v) \
        PROCESS_DECL(void, vec_clear, ptr_vector *v) \
        \
        PROCESS_DECL(void, vec_insert, ptr_vector *v, size_t at, void *value) \
        PROCESS_DECL(void, vec_push_back, ptr_vector *v, void *value) \
        \
        PROCESS_DECL(void*, vec_erase, ptr_vector *v, size_t at) \
        PROCESS_DECL(void*, vec_pop_back, ptr_vector *v)

/** 
 * either a const fptr to linked functions, or fptr to be assigned with dlsym()
 */
#ifdef DYNAMIC
    #define DEF_FPTR(ret, name, ...) ret (* fptr_##name)(__VA_ARGS__);
#else
    #define DEF_FPTR(ret, name, ...) ret (* const fptr_##name)(__VA_ARGS__) = name;
#endif

RRMERGE_FUNCTIONS(DEF_FPTR)

#undef DEF_FPTR 

bool verbose = true;

struct tms tms_measure_start;
clock_t real_measure_start;
bool measure_running = false;

v_v_char row_blocks;

bool is_prefix(char* needle, char* haystack);
bool handle_exit(void);
bool handle_get_ticks_per_sec(void);
bool handle_start_measurement(void);
bool handle_end_measurement(void);
bool handle_print_merged(void);
bool handle_merge_files(char *command);
bool handle_remove_block(char *command);
bool handle_remove_row(char *command);

int main(int argc, char **argv) {
    if (argc <= 2) {
        if (argc == 2) {
            if (strcmp("--quiet", argv[1]) == 0) {
                verbose = false;
            }
        }
    }
    else {
        printf("incorrect number of arguments\n");
        return EXIT_FAILURE;
    }

    #ifdef DYNAMIC
        void *dl_handle = dlopen("librrmerge.so", RTLD_LAZY);
        #define LINK_FPTR(ret, name, ...) fptr_##name = dlsym(dl_handle, #name);

        RRMERGE_FUNCTIONS(LINK_FPTR)

        #undef LINK_FPTR
    #endif

    fptr_vec_init(&row_blocks);

    bool loop = true;
    while (loop) {
        char *command = NULL;
        size_t n = 0;

        if (getline(&command, &n, stdin) == -1) {
            if (verbose) printf("reached eof, aborting\n");
            loop = false;
        }
        else {
            if (strcmp("exit\n", command) == 0) {
                loop = handle_exit();
            }
            else if (strcmp("get_ticks_per_sec\n", command) == 0) {
                loop = handle_get_ticks_per_sec();
            }
            else if (strcmp("start_measurement\n", command) == 0) {
                loop = handle_start_measurement();
            }
            else if (strcmp("end_measurement\n", command) == 0) {
                loop = handle_end_measurement();
            }
            else if (strcmp("print_merged\n", command) == 0) {
                loop = handle_print_merged();
            }
            else if (is_prefix("merge_files", command)) {
                loop = handle_merge_files(command);
            }
            else if (is_prefix("remove_block", command)) {
                loop = handle_remove_block(command);
            }
            else if (is_prefix("remove_row", command)) {
                loop = handle_remove_row(command);
            }
            else {
                printf("unknown command\n");
            }
        }

        free(command);
    }

    fptr_free_row_blocks(&row_blocks);

    #ifdef DYNAMIC
        dlclose(dl_handle);
    #endif

    return EXIT_SUCCESS;
}

bool is_prefix(char* needle, char* haystack) {
    return strncmp(needle, haystack, strlen(needle)) == 0;
}

bool handle_exit(void) {
    if (verbose) printf("aborting\n");

    return false;
}

bool handle_get_ticks_per_sec(void) {
    printf("_SC_CLK_TCK: %ld\n", sysconf(_SC_CLK_TCK));

    return true;
}

bool handle_start_measurement(void) {
    if (measure_running) {
        printf("time measurement is already running\n");
    }
    else {
        if (verbose) printf("starting measurement\n");
        real_measure_start = times(&tms_measure_start);
        
        measure_running = true;
    }

    return true;
}

bool handle_end_measurement(void) {
    if (!measure_running) {
        printf("time measurement was not started\n");
    }
    else {
        struct tms tms_measure_end;
        clock_t real_measure_end = times(&tms_measure_end);

        clock_t real_elapsed = real_measure_end - real_measure_start;
        clock_t user_elapsed = tms_measure_end.tms_utime - tms_measure_start.tms_utime;
        clock_t system_elapsed = tms_measure_end.tms_stime - tms_measure_start.tms_stime;

        if (verbose) printf("measured time: real, user, sys\n");
        printf("%ld %ld %ld\n", real_elapsed, user_elapsed, system_elapsed);

        measure_running = false;
    }

    return true;
}

bool handle_print_merged(void) {
    fptr_print_row_blocks(&row_blocks);
    return true;
}

bool handle_merge_files(char *command) {
    //skip "merge_files"
    char *command_arg = command + strlen("merge_files");

    v_file_pair file_pairs;
    v_FILE tmp_files;

    fptr_vec_init(&file_pairs);
    fptr_vec_init(&tmp_files);

    bool input_valid = true;

    while (input_valid && *command_arg) {
        char *path_pair = NULL;
        int n = 0;

        if (sscanf(command_arg, " %ms %n", &path_pair, &n) != 1 || strchr(path_pair, ':') == NULL) {
            printf("malformed argument\n");
            input_valid = false;
        }
        else {
            fptr_add_file_pair(&file_pairs, path_pair);
        }

        free(path_pair);
        command_arg += n;
    }

    if (input_valid) {
        fptr_merge_file_pairs(&tmp_files, &file_pairs);

        for (size_t i = 0; i < tmp_files.size; i++) {
            fptr_add_row_block(&row_blocks, tmp_files.storage[i]);
        }
    }

    fptr_free_file_pairs(&file_pairs);
    fptr_free_tmp_files(&tmp_files);

    return true;
}

bool handle_remove_block(char *command) {
    //skip "remove_block"
    char *command_arg = command + strlen("remove_block");

    size_t block_idx;
    if (sscanf(command_arg, " %zu ", &block_idx) == 1) {
        if (block_idx < row_blocks.size) {
            fptr_remove_row_block(&row_blocks, block_idx);
        }
        else {
            printf("block idx out of range\n");
        }
    }
    else {
        printf("malformed argument\n");
    }

    return true;
}

bool handle_remove_row(char *command) {
    //skip "remove_row"
    char *command_arg = command + strlen("remove_row");

    size_t block_idx;
    size_t row_idx;
    if (sscanf(command_arg, " %zu %zu ", &block_idx, &row_idx) == 2) {
        if (block_idx < row_blocks.size) {
            v_char *row_block = row_blocks.storage[block_idx];

            if (row_idx < row_block->size) {
                fptr_remove_row(&row_blocks, block_idx, row_idx);
            }
            else {
                printf("row idx out of range\n");
            }
        }
        else {
            printf("block idx out of range\n");
        }
    }
    else {
        printf("malformed argument\n");
    }

    return true;
}