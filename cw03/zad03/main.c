#define _GNU_SOURCE //enable asprintf

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

extern char **environ;
const char* PRIV_ENV_VAR = "_CW03_ZAD03_RECURSIVE_EXEC";

bool kmp_file_contains(FILE *file, const char *needle);

int main(int argc, char **argv) {
    if (argc != 4) {
        perror("invalid argument count\n");
        return EXIT_FAILURE;
    }

    size_t depth;
    if (sscanf(argv[3], "%zu", &depth) != 1) {
        perror("malformed depth argument\n");
        return EXIT_FAILURE;
    }

    //we don't want the dot in paths printed out to the user
    const char *dir_path;
    const char *dir_path_no_dot;

    //detect initial call, change working dir to argv[1] so we can build relative paths properly
    if (getenv(PRIV_ENV_VAR)) {
        dir_path_no_dot = argv[1];
        dir_path = argv[1];
    }
    else {
        if (chdir(argv[1]) == -1) {
            fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
            return EXIT_FAILURE;
        }
        dir_path_no_dot = "";
        dir_path = "./";
        setenv(PRIV_ENV_VAR, "", 1);
    }

    //we'll exec ourselves recursively using fexecve, because we shouldn't rely on argv[0]
    int self_executable_fd = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
    if (self_executable_fd == -1) {
        perror("can't replicate, wt?f\n");
    }

    /**
     * open directory through fd so we can specify O_CLOEXEC to
     * prevent propagation of unecessary fd's into forks
     */
    int dir_fd = open(dir_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd == -1) {
        fprintf(stderr, "%s: %s\n", dir_path, strerror(errno));
        return EXIT_FAILURE;
    }

    DIR *dir = fdopendir(dir_fd);

    struct dirent *curr_dirent;
    while ((curr_dirent = readdir(dir))) {
        if (strcmp(".", curr_dirent->d_name) == 0) continue;
        if (strcmp("..", curr_dirent->d_name) == 0) continue;

        if (curr_dirent->d_type == DT_REG) {
            char *dot = strrchr(curr_dirent->d_name, '.');

            if (dot && strcmp(dot, ".txt") == 0) {
                char *file_path;
                asprintf(&file_path, "%s%s", dir_path_no_dot, curr_dirent->d_name);

                FILE *file = fopen(file_path, "r");
                if (file) {
                    if (kmp_file_contains(file, argv[2])) {
                        printf("%d: %s\n", getpid(), file_path);
                    }

                    fclose(file);
                }
                else {
                    fprintf(stderr, "%s: %s\n", file_path, strerror(errno));
                }

                free(file_path);
            }
        }
        else if (curr_dirent->d_type == DT_DIR && depth > 0) {
            char *subdir_path;
            asprintf(&subdir_path, "%s%s/", dir_path_no_dot, curr_dirent->d_name);

            char new_depth_str[12];
            sprintf(new_depth_str, "%zu", depth - 1);

            char *newargv[] = { argv[0], subdir_path, argv[2], new_depth_str, NULL };

            //flush these files, so they won't be accidentaly duplicated in forks
            fflush(stdout);
            fflush(stderr);

            /**
             * recursive execs go brrr
             * 
             * to be able to close current directory in the fork, we must be in a real fork
             * with working copy-on-write
             * 
             * in real use-case  this is a waste of time and we can fork
             * using vfork to be less resource heavy, just memcheck won't be happy
             * 
             * in case of subdir_path, this is a guaranteed memleak in terms of memcheck,
             * but it is also an argument of fexecve, so there is no way to free it
             * 
             * btw this program is not valgrindable for depth > 0, because
             * self_executable_fd points to valgrind binary. lmao
             * 
             * environ is passed so PRIV_ENV_VAR can propagate with no effort
             */
            #ifdef MEMCHECK_HAPPY
            if (fork() == 0) {
                closedir(dir);

                fexecve(self_executable_fd, newargv, environ);
                _exit(EXIT_FAILURE); //in case exec fails for whatever reason
            }
            #else
            if (vfork() == 0) {
                //only exec and _exit calls are legal here
                fexecve(self_executable_fd, newargv, environ);
                _exit(EXIT_FAILURE); //in case exec fails for whatever reason
            }
            #endif

            free(subdir_path);
        }
    }

    closedir(dir);

    //consume zombies, wait for all children to finish
    while (wait(NULL) > 0)
        ;
}

bool kmp_file_contains(FILE *file, const char *needle) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0) {
        return true;
    }

    size_t *prefix_fun = malloc(needle_len * sizeof(*prefix_fun));
    size_t k = 0;

    prefix_fun[0] = 0;
    for (size_t q = 1; q < needle_len; q++) {
        while (k > 0 && needle[k] != needle[q]) {
            k = prefix_fun[k - 1];
        }

        if (needle[k] == needle[q]) {
            k++;
        }

        prefix_fun[q] = k;
    }

    size_t matches = 0;
    char buf;
    bool result = false;

    while (fread(&buf, sizeof(buf), 1, file) == 1) {
        while (matches > 0 && needle[matches] != buf) {
            matches = prefix_fun[matches - 1];
        }

        if (needle[matches] == buf) {
            matches++;
        }

        if (matches == needle_len) {
            result = true;
            break;
        }
    }

    free(prefix_fun);
    return result;
}
