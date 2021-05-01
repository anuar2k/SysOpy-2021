#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <unistd.h>

#include "chat.h"

volatile sig_atomic_t stdin_received = false;
volatile sig_atomic_t interrupting_sig = 0;

bool connected = false;
bool notify_server = false;
unsigned char client_id;
int other_client_queue_id;

void term_handler(int sig);
void sigio_handler(int sig);

int main(void) {
    char *home_path = getenv("HOME");
    if (!home_path) {
        fprintf(stderr, "home path not defined\n");
        return EXIT_FAILURE;
    }

    key_t server_queue_key = ftok(home_path, CHAT_PROJ_ID);
    if (server_queue_key == -1) {
        perror("server key gen");
        return EXIT_FAILURE;
    }

    int client_queue_id = -1;
    int server_queue_id = -1;

    client_queue_id = msgget(IPC_PRIVATE, 0666);
    if (client_queue_id == -1) {
        perror("client queue");
        goto cleanup;
    }

    server_queue_id = msgget(server_queue_key, 0);
    if (server_queue_id == -1) {
        perror("server queue");
        goto cleanup;
    }

    struct sigaction act;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    act.sa_handler = term_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    act.sa_handler = sigio_handler;
    sigaction(SIGIO, &act, NULL);

    fcntl(STDIN_FILENO, F_SETOWN, getpid());
    fcntl(STDIN_FILENO, F_SETFL, O_ASYNC);

    s_msg_buf initial = {
        .mtype = S_MSG_INIT,
        .msg.init_p.client_queue_id = client_queue_id
    };

    if (msgsnd(server_queue_id, &initial, sizeof(initial), 0) == -1) {
        perror("init");
        goto cleanup;
    }

    while (true) {
        c_msg_buf inc;

        if (msgrcv(client_queue_id, &inc, sizeof(inc), -C_MSG_TYPE_MAX, 0) == -1) {
            if (errno == EINTR) {
                if (interrupting_sig) {
                    fprintf(stderr, "client interrupted by sig %d\n", interrupting_sig);
                    notify_server = true;
                    goto cleanup;
                }
                if (stdin_received) {
                    char *line = NULL;
                    size_t n = 0;
                    getline(&line, &n, stdin);
                    stdin_received = false;

                    if (strcmp("LIST\n", line) == 0) {
                        s_msg_buf msg_buf = {
                            .mtype = S_MSG_LIST,
                            .msg.list_p.client_id = client_id
                        };
                        
                        if (msgsnd(server_queue_id, &msg_buf, sizeof(msg_buf), 0) == -1) {
                            perror("LIST");
                            free(line);
                            goto cleanup;
                        }
                    }
                    else if (strcmp("DISCONNECT\n", line) == 0) {
                        if (!connected) {
                            printf("client not connected\n");
                        }
                        else {
                            s_msg_buf msg_buf = {
                                .mtype = S_MSG_DISCONNECT,
                                .msg.disconnect_p.client_id = client_id
                            };

                            if (msgsnd(server_queue_id, &msg_buf, sizeof(msg_buf), 0) == -1) {
                                perror("DISCONNECT");
                                free(line);
                                goto cleanup;
                            }
                        }
                    }
                    else if (strcmp("STOP\n", line) == 0) {
                        free(line);
                        goto cleanup;
                    }
                    else {
                        char *line_begin = line;
                        char *space = strchr(line, ' ');
                        char *arg = space + 1;
                        if (!space) {
                            fprintf(stderr, "invalid command\n");
                        }

                        *space = '\0';

                        if (strcmp("CONNECT", line_begin) == 0) {
                            unsigned char other_client_id;
                            if (sscanf(arg, "%hhd", &other_client_id) != 1) {
                                fprintf(stderr, "invalid client id\n");
                            }
                            else {
                                if (client_id == other_client_id) {
                                    fprintf(stderr, "you can't connect to yourself\n");
                                }
                                else {
                                    s_msg_buf msg_buf = {
                                        .mtype = S_MSG_CONNECT,
                                        .msg.connect_p.client_id = client_id,
                                        .msg.connect_p.other_client_id = other_client_id
                                    };

                                    if (msgsnd(server_queue_id, &msg_buf, sizeof(msg_buf), 0) == -1) {
                                        perror("CONNECT");
                                        free(line);
                                        notify_server = true;
                                        goto cleanup;
                                    }
                                }
                            }
                        }
                        else if (strcmp("WRITE", line_begin) == 0) {
                            if (!connected) {
                                fprintf(stderr, "client not connected\n");
                            }
                            else {
                                c_msg_buf msg_buf = {
                                    .mtype = C_MSG_MESSAGE
                                };
                                strncpy(msg_buf.msg.message_p.message, arg, MESSAGE_MAX - 1);
                                msg_buf.msg.message_p.message[MESSAGE_MAX - 1] = '\0';

                                if (msgsnd(other_client_queue_id, &msg_buf, sizeof(msg_buf), 0) == -1) {
                                    perror("WRITE");
                                    free(line);
                                    notify_server = true;
                                    goto cleanup;
                                }
                            }
                        }
                        else {
                            fprintf(stderr, "invalid command\n");
                        }
                    }

                    free(line);
                }
                continue;
            }
            else {
                perror("receive");
                notify_server = true;
                goto cleanup;
            }
        }
        else {
            switch (inc.mtype) {
                case C_MSG_DISCONNECT: {
                    connected = false;
                    printf("disconnected\n");
                } break;
                case C_MSG_STOP: {
                    printf("server quitting\n");
                    goto cleanup;
                } break;
                case C_MSG_CONNECT: {
                    printf("connected\n");
                    other_client_queue_id = inc.msg.connect_p.other_client_queue_id;
                    connected = true;
                } break;
                case C_MSG_MESSAGE: {
                    printf("received message: %s", inc.msg.message_p.message);
                } break;
                case C_MSG_LIST: {
                    printf("%zu clients total\n", inc.msg.list_p.clients_count);
                    for (unsigned char other_client = 0; other_client < CLIENTS_MAX; other_client++) {
                        if (inc.msg.list_p.clients[other_client] != ST_EMPTY) {
                            printf("%2hhd: %s\n", other_client, (inc.msg.list_p.clients[other_client] == ST_AVAIL ? "available" : "busy"));
                        }
                    }
                } break;
                case C_MSG_INIT: {
                    client_id = inc.msg.init_p.client_id;
                    if (client_id == -1) {
                        fprintf(stderr, "server full\n");
                        goto cleanup;
                    }
                    else {
                        printf("assigned client id %hhd\n", client_id);
                    }
                } break;
                default: break;
            }
        }
    }
    
    cleanup:
    if (notify_server) {
        s_msg_buf msg_buf = {
            .mtype = S_MSG_STOP,
            .msg.stop_p.client_id = client_id
        };

        msgsnd(server_queue_id, &msg_buf, sizeof(msg_buf), 0);
    }
    msgctl(client_queue_id, IPC_RMID, NULL);
}

void term_handler(int sig) {
    interrupting_sig = sig;
}

void sigio_handler(int sig) {
    stdin_received = true;
}
