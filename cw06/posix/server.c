#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <mqueue.h>

#include "chat.h"

volatile sig_atomic_t interrupting_sig = 0;

size_t clients_count = 0;

client_status clients[CLIENTS_MAX] = { [0 ... CLIENTS_MAX - 1] = ST_EMPTY };
char client_queue_names[CLIENTS_MAX][QUEUE_NAME_MAX];
mqd_t client_queue_mqds[CLIENTS_MAX];
unsigned char connected_to[CLIENTS_MAX];

unsigned char find_free_client_id(void);
void term_handler(int sig);

int main(void) {
    struct mq_attr attr = {
        .mq_maxmsg = 10,
        .mq_msgsize = sizeof(s_msg_buf)
    };

    mqd_t server_queue = mq_open(QUEUE_NAME_PREFIX "server", O_RDWR | O_CREAT | O_EXCL, 0666, &attr);
    if (server_queue == -1) {
        perror("server queue");
        return EXIT_FAILURE;
    }

    mq_getattr(server_queue, &attr);
    printf("%zu %zu %zu %zu\n", attr.mq_curmsgs, attr.mq_flags, attr.mq_maxmsg, attr.mq_msgsize);

    struct sigaction act;
    act.sa_handler = term_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    while (true) {
        s_msg_buf inc;

        if (mq_receive(server_queue, (char *)&inc, sizeof(inc), NULL) == -1) {
            if (errno == EINTR) {
                if (interrupting_sig) {
                    fprintf(stderr, "server interrupted by sig %d\n", interrupting_sig);
                    goto cleanup;
                }
                continue;
            }
            else {
                perror("receive");
                goto cleanup;
            }
        }
        else {
            switch (inc.mtype) {
                case S_MSG_DISCONNECT: {
                    unsigned char client_id = inc.msg.disconnect_p.client_id;

                    if (clients[client_id] != ST_BUSY) {
                        fprintf(stderr, "client not busy\n");
                        continue;
                    }

                    clients[client_id] = ST_AVAIL;
                    clients[connected_to[client_id]] = ST_AVAIL;

                    c_msg_buf msg_buf = {
                        .mtype = C_MSG_DISCONNECT
                    };

                    if (mq_send(client_queue_mqds[connected_to[client_id]], (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype) == -1) {
                        perror("S_MSG_DISCONNECT");
                    }
                } break;
                case S_MSG_STOP: {
                    unsigned char client_id = inc.msg.stop_p.client_id;

                    switch (clients[client_id]) {
                        case ST_EMPTY: {
                            fprintf(stderr, "no such client\n");
                        } break;
                        case ST_AVAIL: {
                            clients[client_id] = ST_EMPTY;
                            mq_close(client_queue_mqds[client_id]);
                            clients_count--;
                        } break;
                        case ST_BUSY: {
                            clients[client_id] = ST_EMPTY;
                            mq_close(client_queue_mqds[client_id]);
                            clients[connected_to[client_id]] = ST_AVAIL;

                            c_msg_buf msg_buf = {
                                .mtype = C_MSG_DISCONNECT
                            };

                            if (mq_send(client_queue_mqds[connected_to[client_id]], (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype) == -1) {
                                perror("S_MSG_STOP");
                            }

                            clients_count--;
                        } break;
                        default: break;
                    }
                } break;
                case S_MSG_CONNECT: {
                    unsigned char client_id = inc.msg.connect_p.client_id;
                    unsigned char other_client_id = inc.msg.connect_p.other_client_id;

                    if (clients[client_id] == ST_EMPTY || clients[other_client_id] == ST_EMPTY) {
                        fprintf(stderr, "unknown client\n");
                        continue;
                    }

                    if (client_id == other_client_id) {
                        fprintf(stderr, "a client can't connect to itself\n");
                        continue;
                    }

                    if (clients[client_id] == ST_BUSY || clients[other_client_id] == ST_BUSY) {
                        fprintf(stderr, "at least one client is busy\n");
                        continue;
                    }

                    c_msg_buf msg_buf = {
                        .mtype = C_MSG_CONNECT
                    };

                    clients[client_id] = ST_BUSY;
                    clients[other_client_id] = ST_BUSY;
                    connected_to[client_id] = other_client_id;
                    connected_to[other_client_id] = client_id;

                    strcpy(msg_buf.msg.connect_p.other_client_queue_name, client_queue_names[other_client_id]);
                    if (mq_send(client_queue_mqds[client_id], (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype) == -1) {
                        perror("S_MSG_CONNECT");
                        continue;
                    }

                    strcpy(msg_buf.msg.connect_p.other_client_queue_name, client_queue_names[client_id]);
                    if (mq_send(client_queue_mqds[other_client_id], (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype) == -1) {
                        perror("S_MSG_CONNECT");
                    }
                } break;
                case S_MSG_LIST: {
                    unsigned char client_id = inc.msg.list_p.client_id;

                    if (clients[client_id] == ST_EMPTY) {
                        fprintf(stderr, "unknown client\n");
                        continue;
                    }

                    c_msg_buf msg_buf = {
                        .mtype = C_MSG_LIST,
                        .msg.list_p.clients_count = clients_count
                    };
                    
                    memcpy(&msg_buf.msg.list_p.clients, &clients, sizeof(clients));

                    if (mq_send(client_queue_mqds[client_id], (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype) == -1) {
                        perror("S_MSG_LIST");
                    }
                } break;
                case S_MSG_INIT: {
                    c_msg_buf msg_buf = {
                        .mtype = C_MSG_INIT
                    };

                    if (clients_count == CLIENTS_MAX) {
                        msg_buf.msg.init_p.client_id = -1;
                        mqd_t new_client = mq_open(inc.msg.init_p.client_queue_name, O_RDWR);
                        if (mq_send(new_client, (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype) == -1) {
                            perror("S_MSG_INIT");
                        }
                        mq_close(new_client);
                    }
                    else {
                        unsigned char new_client_id = find_free_client_id();
                        strcpy(client_queue_names[new_client_id], inc.msg.init_p.client_queue_name);
                        client_queue_mqds[new_client_id] = mq_open(client_queue_names[new_client_id], O_RDWR);
                        clients[new_client_id] = ST_AVAIL;

                        msg_buf.msg.init_p.client_id = new_client_id;
                        clients_count++;

                        if (mq_send(client_queue_mqds[new_client_id], (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype) == -1) {
                            perror("S_MSG_INIT");
                        }
                    }

                } break;
                default: break;
            }
        }
    }

    cleanup:
    for (unsigned char client_id = 0; client_id < CLIENTS_MAX; client_id++) {
        if (clients[client_id] != ST_EMPTY) {
            c_msg_buf msg_buf = {
                .mtype = C_MSG_STOP
            };

            mq_send(client_queue_mqds[client_id], (char *)&msg_buf, sizeof(msg_buf), msg_buf.mtype);
            mq_close(client_queue_mqds[client_id]);
        }
    }
    mq_close(server_queue);
    mq_unlink(QUEUE_NAME_PREFIX "server");
}

unsigned char find_free_client_id(void) {
    unsigned char client_id = 0;
    while (clients[client_id] != ST_EMPTY) {
        client_id++;
    }
    return client_id;
}

void term_handler(int sig) {
    interrupting_sig = sig;
}
