#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>

#include "tictactoe.h"

int sock = -1;
sem_t input_move_sem;

void *receiver_thread(void *arg);
void print_game(game *g);

int main(int argc, char **argv) {
    sem_init(&input_move_sem, false, 0);

    const char *username = argv[1];
    const char *mode = argv[2];
    if (argc == 5) {
        if (strcmp(mode, "interwebz") == 0) {
            const char *ip = argv[3];
            const char *port = argv[4];

            struct addrinfo hints;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            struct addrinfo *res;
            int gai_error;
            if ((gai_error = getaddrinfo(ip, port, &hints, &res)) != 0) {
                puts(gai_strerror(gai_error));
                return EXIT_FAILURE;
            }
            struct sockaddr_in *interwebz_addr = (struct sockaddr_in *)res->ai_addr;
            socklen_t interwebz_addr_len = res->ai_addrlen;

            int s = socket(AF_INET, SOCK_STREAM, 0);
            if (s == -1) {
                perror("socket");
                return EXIT_FAILURE;
            }

            if (connect(s, (struct sockaddr*)interwebz_addr, interwebz_addr_len) == -1) {
                perror("connect");
                return EXIT_FAILURE;
            }

            freeaddrinfo(res);
            sock = s;
        }
    }
    else if (argc == 4) {
        if (strcmp(mode, "unix") == 0) {
            const char *unix_socket_path = argv[3];
            struct sockaddr_un unix_addr = {
                .sun_family = AF_UNIX,
            };
            strcpy(unix_addr.sun_path, unix_socket_path);

            int s = socket(AF_UNIX, SOCK_STREAM, 0);
            if (s == -1) {
                perror("socket");
                return EXIT_FAILURE;
            }

            if (connect(s, (struct sockaddr*)&unix_addr, sizeof(unix_addr)) == -1) {
                perror("connect");
                return EXIT_FAILURE;
            }

            sock = s;
        }
    }
    else {
        fprintf(stderr, "invalid argument count\n");
        return EXIT_FAILURE;
    }

    if (sock == -1) {
        fprintf(stderr, "invalid arguments\n");
        return EXIT_FAILURE;
    }

    pthread_t thr;
    pthread_create(&thr, NULL, receiver_thread, NULL);

    //poor man's packed struct
    const size_t username_buf_len = sizeof(packet_header) + USERNAME_LEN_MAX;
    char username_buf[username_buf_len];
    *((packet_header*)username_buf) = PH_HELLO;
    strcpy(username_buf + sizeof(packet_header), username);

    write(sock, username_buf, username_buf_len);

    while (true) {
        sem_wait(&input_move_sem);
        char move_char;
        scanf(" %c", &move_char);
        unsigned char move = move_char - '0';

        //poor man's packed struct
        const size_t move_buf_len = sizeof(packet_header) + sizeof(unsigned char);
        char move_buf[move_buf_len];
        *((packet_header*)move_buf) = PH_MOVE;
        *((unsigned char*)&move_buf[sizeof(packet_header)]) = move;

        write(sock, &move_buf, move_buf_len);
    }
}

void *receiver_thread(void *arg) {
    game g;
    for (size_t i = 0; i < 9; i++) {
        g.board[i] = '-';
    }
    g.x_next = true;

    while (true) {
        packet_header header;
        ssize_t count = read(sock, &header, sizeof(header));
        if (count == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        if (count == 0) {
            fprintf(stderr, "server disconnected\n");
            exit(EXIT_SUCCESS);
        }

        switch (header) {
            case PH_FULL: {
                fprintf(stderr, "server full\n");
                exit(EXIT_FAILURE);
            } break;
            case PH_BAD_UNAME: {
                fprintf(stderr, "username not unique\n");
                exit(EXIT_FAILURE);
            } break;
            case PH_BEGIN_X: {
                printf("Game begins, you move first!\n");
                print_game(&g);
                sem_post(&input_move_sem);
            } break;
            case PH_BEGIN_O: {
                printf("Game begins, wait for the move!\n");
            } break;
            case PH_MOVE: {
                read(sock, &g, sizeof(g));
                printf("Opponent moved, please move!\n");
                print_game(&g);
                sem_post(&input_move_sem);
            } break;
            case PH_WIN: {
                printf("you won!\n");
                exit(EXIT_SUCCESS);
            } break;
            case PH_LOSE: {
                printf("you lost!\n");
                exit(EXIT_SUCCESS);
            } break;
            case PH_TIE: {
                printf("you tied!\n");
                exit(EXIT_SUCCESS);
            } break;
            case PH_PING: {
                static const packet_header ping = PH_PING;
                write(sock, &ping, sizeof(ping));
            } break;
            case PH_OPP_DC: {
                printf("opponent disconnected\n");
                exit(EXIT_SUCCESS);
            } break;
            default: {
                fprintf(stderr, "invalid packet type\n");
                exit(EXIT_SUCCESS);
            } break;
        }
    }

    return NULL;
}

void print_game(game *g) {
    printf("012   %c%c%c\n", g->board[0], g->board[1], g->board[2]);
    printf("345   %c%c%c\n", g->board[3], g->board[4], g->board[5]);
    printf("678   %c%c%c\n", g->board[6], g->board[7], g->board[8]);
}
