#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>

#include "chat.h"

volatile sig_atomic_t stdin_received = false;
volatile sig_atomic_t interrupting_sig = 0;

bool connected = false;
bool notify_server = false;
unsigned char client_id;
char other_client_queue_name[QUEUE_NAME_MAX];
mqd_t other_client_queue;

void term_handler(int sig);
void sigio_handler(int sig);

int main(void) {
    char client_queue_name[QUEUE_NAME_MAX];
    sprintf(client_queue_name, "%s%d", QUEUE_NAME_PREFIX, getpid());

    struct mq_attr attr = {
        .mq_maxmsg = 10,
        .mq_msgsize = sizeof(c_msg_buf)
    };

    mqd_t client_queue = -1;
    mqd_t server_queue = -1;

    client_queue = mq_open(client_queue_name, O_RDWR | O_CREAT | O_EXCL, 0666, &attr);
    if (client_queue == -1) {
        perror("client queue");
        goto cleanup;
    }

    server_queue = mq_open(QUEUE_NAME_PREFIX "server", O_RDWR);
    if (server_queue == -1) {
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
        .mtype = S_MSG_INIT
    };

    strcpy(initial.msg.init_p.client_queue_name, client_queue_name);

    if (mq_send(server_queue, (char *)&initial, sizeof(initial), initial.mtype) == -1) {
        perror("init");
        goto cleanup;
    }

    while (true) {
        c_msg_buf inc;

        if (mq_receive(client_queue, (char *)&inc, sizeof(inc), NULL) == -1) {
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

                    if (strcmp("LIST\n", line) == 0) {
                        s_msg_buf msg_buf = {
                            .mtype = S_MSG_LIST,
                            .msg.list_p.client_id = client_id
                        };

                        if (mq_send(server_queue, (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype) == -1) {
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

                            if (mq_send(server_queue, (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype) == -1) {
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

                                    if (mq_send(server_queue, (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype) == -1) {
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

                                if (mq_send(other_client_queue, (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype) == -1) {
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
                    mq_close(other_client_queue);
                    printf("disconnected\n");
                } break;
                case C_MSG_STOP: {
                    printf("server quitting\n");
                    goto cleanup;
                } break;
                case C_MSG_CONNECT: {
                    printf("connected\n");
                    strcpy(other_client_queue_name, inc.msg.connect_p.other_client_queue_name);
                    other_client_queue = mq_open(other_client_queue_name, O_RDWR);
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

        mq_send(server_queue, (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype);
    }

    mq_close(server_queue);
    mq_close(client_queue);
    mq_unlink(client_queue_name);
}

void term_handler(int sig) {
    interrupting_sig = sig;
}

void sigio_handler(int sig) {
    stdin_received = true;
}
