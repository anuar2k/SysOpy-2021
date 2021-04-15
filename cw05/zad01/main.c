#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ptr_vector.h"

#define IN 0
#define OUT 1

//vector of char*
typedef ptr_vector v_char;
//vector of vector of char*
typedef ptr_vector v_v_char;
//vector of named_pipe
typedef ptr_vector v_np;

typedef struct {
    char *name;
    /**
     * first dim is whole commands
     * second dim is split by args
     * second dim is suitable to pass to exec (i.e first element is the command and the array is NULL-terminated)
     */
    v_v_char exec_args;
} named_pipe;

const char *COMMAND_DELIM = "|";
const char *ARG_DELIM = " \f\n\r\t\v"; //see man isspace

int named_pipe_ptr_cmp(const void *p1, const void *p2) {
    named_pipe * const *c1 = p1;
    named_pipe * const *c2 = p2;

    return strcmp((*c1)->name, (*c2)->name);
}

int named_pipe_search_cmp(const void *key, const void *arr_el) {
    char * const *key_str = key;
    named_pipe * const *pipe = arr_el;

    return strcmp(*key_str, (*pipe)->name);
}

void free_n(size_t n, ...) {
    va_list ptrs;
    va_start(ptrs, n);

    while (n-- > 0) {
        free(va_arg(ptrs, void*));
    }

    va_end(ptrs);
}

void free_v_char_content(v_char *v) {
    while (v->size > 0) {
        free(vec_pop_back(v));
    }
}

void free_v_v_char_content(v_v_char *v) {
    while (v->size > 0) {
        v_char *vc = vec_pop_back(v);
        free_v_char_content(vc);
        free(vc);
    }
}

void free_pipes(v_np *pipes) {
    while (pipes->size > 0) {
        named_pipe *pipe = vec_pop_back(pipes);

        free_v_v_char_content(&pipe->exec_args);
        free_n(2, pipe->name, pipe);
    }
}

//to - exclusive
char *copy_stripped(const char *from, const char *to) {
    while (from < to && isspace(*from)) {
        from++;
    }
    while (from < to && isspace(*(to - 1))) {
        to--;
    }

    size_t len = to - from;

    char *result = malloc((len + 1) * sizeof(*result));
    memcpy(result, from, len * sizeof(*result));
    result[len] = '\0';

    return result;
}

int main(int argc, char **argv) {
    int result = EXIT_FAILURE;

    if (argc != 2) {
        fprintf(stderr, "invalid argument count\n");
        return result;
    }

    FILE *input = fopen(argv[1], "r");
    if (!input) {
        perror(argv[1]);
        return result;
    }

    v_np pipes;
    vec_init(&pipes);

    bool definitions = true;

    char *line = NULL;
    size_t n = 0;
    ssize_t line_no = -1;
    while (getline(&line, &n, input) != -1) {
        line_no++;

        if (line[0] == '\n') {
            if (definitions) {
                definitions = false;
                qsort(pipes.storage, pipes.size, sizeof(*pipes.storage), named_pipe_ptr_cmp);
            }
            continue;
        }

        if (definitions) {
            char *eq_char = strchr(line, '=');

            if (eq_char) {
                named_pipe *pipe = malloc(sizeof(*pipe));
                vec_init(&pipe->exec_args);
                pipe->name = copy_stripped(line, eq_char);

                vec_push_back(&pipes, pipe);

                if (pipe->name[0] == '\0') {
                    fprintf(stderr, "line %zd: empty definition name\n", line_no);

                    goto cleanup;
                }

                char *pipe_def = eq_char + 1;

                char *command_saveptr = NULL;
                char *command = strtok_r(pipe_def, COMMAND_DELIM, &command_saveptr);

                if (!command) {
                    fprintf(stderr, "line %zd: empty command def\n", line_no);

                    goto cleanup;
                }

                while (command) {
                    char *arg_saveptr = NULL;
                    char *arg = strtok_r(command, ARG_DELIM, &arg_saveptr);

                    if (!arg) {
                        fprintf(stderr, "line %zd, empty args def\n", line_no);

                        goto cleanup;
                    }

                    v_char *args = malloc(sizeof(*args));
                    vec_init(args);
                    while (arg) {
                        vec_push_back(args, strdup(arg));
                        arg = strtok_r(NULL, ARG_DELIM, &arg_saveptr);
                    }
                    vec_push_back(args, NULL); //make suitable to pass to exec

                    vec_push_back(&pipe->exec_args, args);
                    command = strtok_r(NULL, COMMAND_DELIM, &command_saveptr);
                }
            }
            else {
                fprintf(stderr, "line %zd: malformed definition\n", line_no);

                goto cleanup;
            }
        }
        else {
            v_v_char exec_args_shallow; //we'll concat command lists, by shallow copy
            vec_init(&exec_args_shallow);

            char *name_saveptr = NULL;
            char *name = strtok_r(line, COMMAND_DELIM, &name_saveptr);

            if (!name) {
                fprintf(stderr, "line %zd: empty named pipe chain\n", line_no);

                goto cleanup;
            }

            while (name) {
                char *stripped_name_saveptr = NULL;
                char *stripped_name = strtok_r(name, ARG_DELIM, &stripped_name_saveptr);

                if (!stripped_name) {
                    fprintf(stderr, "line %zd: empty named pipe in chain\n", line_no);

                    vec_clear(&exec_args_shallow);
                    goto cleanup;
                }

                named_pipe **pipe_ptr = bsearch(
                    &stripped_name,
                    pipes.storage,
                    pipes.size,
                    sizeof(*pipes.storage),
                    named_pipe_search_cmp
                );

                if (!pipe_ptr) {
                    fprintf(stderr, "line %zd: named pipe not found\n", line_no);

                    vec_clear(&exec_args_shallow);
                    goto cleanup;
                }

                named_pipe *pipe = *pipe_ptr;
                for (size_t i = 0; i < pipe->exec_args.size; i++) {
                    vec_push_back(&exec_args_shallow, pipe->exec_args.storage[i]);
                }

                name = strtok_r(NULL, COMMAND_DELIM, &name_saveptr);
            }

            int stdout_inject;
            for (ssize_t i = exec_args_shallow.size - 1; i >= 0; i--) {
                v_char *args_vec = exec_args_shallow.storage[i];
                char **args = (char**)args_vec->storage;

                int fd[2];
                if (i > 0) {
                    pipe(fd);
                }

                if (fork() == 0) {
                    if (i < exec_args_shallow.size - 1) {
                        dup2(stdout_inject, STDOUT_FILENO);
                    }
                    if (i > 0) {
                        close(fd[OUT]);
                        dup2(fd[IN], STDIN_FILENO);
                    }
                    execvp(args[0], args);
                    goto cleanup; //in case it fails
                }

                if (i < exec_args_shallow.size - 1) {
                    close(stdout_inject);
                }
                if (i > 0) {
                    close(fd[IN]);
                    stdout_inject = fd[OUT];
                }
                //should we close any files?
            }

            while (wait(NULL) > 0)
                ;

            vec_clear(&exec_args_shallow);
        }
    }

    result = EXIT_SUCCESS;

    cleanup:
    free(line);
    free_pipes(&pipes);
    fclose(input);

    return result;
}
