#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "chat.h"

volatile sig_atomic_t interrupting_sig = 0;

size_t clients_count = 0;

client_status clients[CLIENTS_MAX] = { [0 ... CLIENTS_MAX - 1] = ST_EMPTY };
int client_queue_ids[CLIENTS_MAX];
unsigned char connected_to[CLIENTS_MAX];

unsigned char find_free_client_id(void);
void term_handler(int sig);

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

    int server_queue_id = msgget(server_queue_key, IPC_CREAT | IPC_EXCL | 0666);
    if (server_queue_id == -1) {
        perror("server queue");
        return EXIT_FAILURE;
    }

    struct sigaction act;
    act.sa_handler = term_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    while (true) {
        s_msg_buf inc;

        if (msgrcv(server_queue_id, &inc, sizeof(inc), -S_MSG_TYPE_MAX, 0) == -1) {
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

                    if (msgsnd(client_queue_ids[connected_to[client_id]], &msg_buf, sizeof(msg_buf), 0) == -1) {
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
                            clients_count--;
                        } break;
                        case ST_BUSY: {
                            clients[client_id] = ST_EMPTY;
                            clients[connected_to[client_id]] = ST_AVAIL;

                            c_msg_buf msg_buf = {
                                .mtype = C_MSG_DISCONNECT
                            };

                            if (msgsnd(client_queue_ids[connected_to[client_id]], &msg_buf, sizeof(msg_buf), 0) == -1) {
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

                    msg_buf.msg.connect_p.other_client_queue_id = client_queue_ids[other_client_id];
                    if (msgsnd(client_queue_ids[client_id], &msg_buf, sizeof(msg_buf), 0) == -1) {
                        perror("S_MSG_CONNECT");
                        continue;
                    }

                    msg_buf.msg.connect_p.other_client_queue_id = client_queue_ids[client_id];
                    if (msgsnd(client_queue_ids[other_client_id], &msg_buf, sizeof(msg_buf), 0) == -1) {
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

                    if (msgsnd(client_queue_ids[client_id], &msg_buf, sizeof(msg_buf), 0) == -1) {
                        perror("S_MSG_LIST");
                    }
                } break;
                case S_MSG_INIT: {
                    c_msg_buf msg_buf = {
                        .mtype = C_MSG_INIT
                    };

                    if (clients_count == CLIENTS_MAX) {
                        msg_buf.msg.init_p.client_id = -1;
                    }
                    else {
                        unsigned char new_client_id = find_free_client_id();
                        client_queue_ids[new_client_id] = inc.msg.init_p.client_queue_id;
                        clients[new_client_id] = ST_AVAIL;

                        msg_buf.msg.init_p.client_id = new_client_id;
                        clients_count++;
                    }

                    if (msgsnd(inc.msg.init_p.client_queue_id, &msg_buf, sizeof(msg_buf), 0) == -1) {
                        perror("S_MSG_INIT");
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

            msgsnd(client_queue_ids[client_id], &msg_buf, sizeof(msg_buf), 0);
        }
    }
    msgctl(server_queue_id, IPC_RMID, NULL);
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
