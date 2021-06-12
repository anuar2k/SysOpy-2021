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

#include "tictactoe.h"

#define BACKLOG_MAX 8
#define PLAYER_COUNT_MAX 32

typedef enum {
    PS_EMPTY,
    PS_QUEUED,
    PS_PLAYING
} player_status;

struct player {
    player_status status;
    int socket;
    struct sockaddr addr;
    socklen_t addr_len;
    bool alive;
    char username[USERNAME_LEN_MAX];
    struct player *opponent;
    game *game;
};
typedef struct player player;

int epoll;

player players[PLAYER_COUNT_MAX] = { 
    [0 ... PLAYER_COUNT_MAX - 1] = {
        .status = PS_EMPTY
    } 
};
pthread_mutex_t players_mut = PTHREAD_MUTEX_INITIALIZER;

void enable_incoming(int epoll, int socket, void *addr, size_t addr_len);
player *find_free_player_spot(void);
player *create_player(int socket, struct sockaddr *addr, socklen_t addr_len, const char *username);
player *get_queued_player(void);
void handle_message(int socket);
void delete_player(player *p);
void *pinger_thread(void *arg);
bool check_win(char board[9], char last_player);
bool check_tie(char board[9]);
bool username_used(const char *username);
player *get_player_for_addr(int socket, struct sockaddr *addr, socklen_t addr_len);

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "invalid argument count\n");
        return EXIT_FAILURE;
    }

    const char *port = argv[1];
    const char *unix_socket_path = argv[2];

    epoll = epoll_create1(0);
    if (epoll == -1) {
        perror("epoll_create");
        return EXIT_FAILURE;
    }

    //unix addr
    struct sockaddr_un unix_addr = {
        .sun_family = AF_UNIX,
    };
    strcpy(unix_addr.sun_path, unix_socket_path);

    //interwebz addr
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo *res;
    int gai_error;
    if ((gai_error = getaddrinfo("localhost", port, &hints, &res)) != 0) {
        puts(gai_strerror(gai_error));
        return EXIT_FAILURE;
    }
    struct sockaddr_in *interwebz_addr = (struct sockaddr_in *)res->ai_addr;
    socklen_t interwebz_addr_len = res->ai_addrlen;

    //listen for incoming connections
    unlink(unix_socket_path);
    int unix_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (unix_socket == -1) {
        perror("unix_socket");
        return EXIT_FAILURE;
    }
    int interwebz_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (interwebz_socket == -1) {
        perror("interwebz_socket");
        return EXIT_FAILURE;
    }

    enable_incoming(epoll, unix_socket, &unix_addr, sizeof(unix_addr));
    enable_incoming(epoll, interwebz_socket, interwebz_addr, interwebz_addr_len);

    freeaddrinfo(res);

    pthread_t thr;
    pthread_create(&thr, NULL, pinger_thread, NULL);

    while (true) {
        struct epoll_event event;
        if (epoll_wait(epoll, &event, 1, -1) == -1) {
            perror("epoll_wait");
            return EXIT_FAILURE;
        }
        
        handle_message(event.data.fd);
    }
}

void enable_incoming(int epoll, int socket, void *addr, size_t addr_len) {
    if (bind(socket, addr, addr_len) != 0) {
        perror("bind");
        exit(1);
    }

    struct epoll_event event = {
        .events = EPOLLIN | EPOLLPRI,
        .data.fd = socket
    };

    if (epoll_ctl(epoll, EPOLL_CTL_ADD, socket, &event) != 0) {
        perror("epoll_ctl");
        exit(1);
    }
}

player *find_free_player_spot(void) {
    for (size_t i = 0; i < PLAYER_COUNT_MAX; i++) {
        if (players[i].status == PS_EMPTY) {
            return &players[i];
        }
    }

    return NULL;
}

player *create_player(int socket, struct sockaddr *addr, socklen_t addr_len, const char *username) {
    player *p = find_free_player_spot();
    if (p == NULL) {
        return NULL;
    }

    p->status = PS_QUEUED;
    p->socket = socket;
    p->addr = *addr;
    p->addr_len = addr_len;
    p->alive = true;
    p->opponent = NULL;
    p->game = NULL;
    strcpy(p->username, username);
    return p;
}

player *get_queued_player(void) {
    for (size_t i = 0; i < PLAYER_COUNT_MAX; i++) {
        if (players[i].status == PS_QUEUED) {
            return &players[i];
        }
    }

    return NULL;
}

void handle_message(int socket) {
    packet_header header;
    struct sockaddr addr;
    socklen_t addr_len;
    recvfrom(socket, &header, sizeof(header), MSG_PEEK, &addr, &addr_len);

    switch (header) {
        case PH_HELLO: {
            const size_t username_buf_len = sizeof(packet_header) + USERNAME_LEN_MAX;
            char username_buf[username_buf_len];
            recvfrom(socket, username_buf, username_buf_len, 0, &addr, &addr_len);

            const char *new_username = username_buf + sizeof(packet_header);
            if (username_used(new_username)) {
                static const packet_header bad_uname = PH_BAD_UNAME;
                sendto(socket, &bad_uname, sizeof(bad_uname), 0, &addr, addr_len);
                break;
            }

            player *possible_opponent = get_queued_player();
            player *p = create_player(socket, &addr, addr_len, new_username);
            if (p == NULL) {
                static const packet_header full = PH_FULL;
                sendto(socket, &full, sizeof(full), 0, &addr, addr_len);
                break;
            }

            if (possible_opponent != NULL) {
                game *g = malloc(sizeof(*g));
                for (size_t i = 0; i < 9; i++) {
                    g->board[i] = '-';
                }
                g->x_next = true;

                p->opponent = possible_opponent;
                possible_opponent->opponent = p;
                p->game = g;
                possible_opponent->game = g;
                p->status = PS_PLAYING;
                possible_opponent->status = PS_PLAYING;

                static const packet_header begin_x = PH_BEGIN_X;
                static const packet_header begin_o = PH_BEGIN_O;
                if (rand() % 2 == 0) {
                    sendto(socket, &begin_x, sizeof(begin_x), 0, &addr, addr_len);
                    sendto(p->opponent->socket, &begin_o, sizeof(begin_o), 0, &p->opponent->addr, p->opponent->addr_len);
                }
                else {
                    sendto(socket, &begin_o, sizeof(begin_o), 0, &addr, addr_len);
                    sendto(p->opponent->socket, &begin_x, sizeof(begin_x), 0, &p->opponent->addr, p->opponent->addr_len);
                }
            }
            else {
                p->status = PS_QUEUED;
            }
        } break;
        case PH_MOVE: {
            const size_t move_buf_len = sizeof(packet_header) + sizeof(unsigned char);
            char move_buf[move_buf_len];
            recvfrom(socket, move_buf, move_buf_len, 0, &addr, &addr_len);
            unsigned char move = *((unsigned char*)&move_buf[sizeof(packet_header)]);

            player *p = get_player_for_addr(socket, &addr, addr_len);
            if (p == NULL) {
                fprintf(stderr, "invalid player address\n");
                break;
            }
            char last_player = p->game->x_next ? 'X' : 'O';
            p->game->board[move] = last_player;

            if (check_win(p->game->board, last_player)) {
                static const packet_header win = PH_WIN;
                static const packet_header lose = PH_LOSE;

                sendto(socket, &win, sizeof(win), 0, &addr, addr_len);
                sendto(p->opponent->socket, &lose, sizeof(lose), 0, &p->opponent->addr, p->opponent->addr_len);
                delete_player(p);
                delete_player(p->opponent);
            }
            else if (check_tie(p->game->board)) {
                static const packet_header tie = PH_TIE;
                
                sendto(socket, &tie, sizeof(tie), 0, &addr, addr_len);
                sendto(p->opponent->socket, &tie, sizeof(tie), 0, &p->opponent->addr, p->opponent->addr_len);
                delete_player(p);
                delete_player(p->opponent);
            }
            else {
                p->game->x_next = !p->game->x_next;

                //poor man's packed struct...
                static const size_t buf_size = sizeof(packet_header) + sizeof(game);
                char buf[buf_size];
                
                *((packet_header*)&buf[0]) = PH_MOVE;
                *((game*)&buf[sizeof(packet_header)]) = *p->game;

                sendto(p->opponent->socket, &buf, buf_size, 0, &p->opponent->addr, p->opponent->addr_len);
            }
        } break;
        case PH_PING: {
            //actually remove the packet from the queue
            recvfrom(socket, &header, sizeof(header), 0, &addr, &addr_len);

            player *p = get_player_for_addr(socket, &addr, addr_len);
            if (p == NULL) {
                fprintf(stderr, "invalid player address\n");
                break;
            }
            p->alive = true;
            printf("received ping\n");
        } break;
        default: {
            //actually remove the packet from the queue
            recvfrom(socket, &header, sizeof(header), 0, &addr, &addr_len);
            player *p = get_player_for_addr(socket, &addr, addr_len);
            if (p == NULL) {
                fprintf(stderr, "invalid player address\n");
                break;
            }
            delete_player(p);
        } break;
    }
}

void delete_player(player *p) {
    if (p->status == PS_PLAYING && p->opponent != NULL) {
        static const packet_header opp_dc = PH_OPP_DC;
        free(p->game);
        p->opponent->game = NULL;
        p->opponent->opponent = NULL;
        sendto(p->opponent->socket, &opp_dc, sizeof(opp_dc), 0, &p->opponent->addr, p->opponent->addr_len);
    }

    p->status = PS_EMPTY;
}

void *pinger_thread(void *arg) {
    static const packet_header ping = PH_PING;

    while (true) {
        sleep(PING_KEEPALIVE);
        pthread_mutex_lock(&players_mut);
        for (size_t i = 0; i < PLAYER_COUNT_MAX; i++) {
            if (players[i].status != PS_EMPTY) {
                player *p = &players[i];

                if (p->alive) {
                    p->alive = false;
                    sendto(p->socket, &ping, sizeof(ping), 0, &p->addr, p->addr_len);
                }
                else {
                    delete_player(p);
                }
            }
        }
        pthread_mutex_unlock(&players_mut);
    }

    return NULL;
}

bool check_win(char board[9], char last_player) {
    static const size_t win_patterns[8][3] = {
        {0, 1, 2},
        {3, 4, 5},
        {6, 7, 8},
        {0, 3, 6},
        {1, 4, 7},
        {2, 5, 8},
        {0, 4, 8},
        {2, 4, 6}
    };

    for (size_t i = 0; i < 8; i++) {
        bool all_match = true;
        for (size_t j = 0; j < 3; j++) {
            if (board[win_patterns[i][j]] != last_player) {
                all_match = false;
                break;
            }
        }

        if (all_match) {
            return true;
        }
    }

    return false;
}

bool check_tie(char board[9]) {
    for (size_t i = 0; i < 9; i++) {
        if (board[i] == '-') {
            return false;
        }
    }

    return true;
}

bool username_used(const char *username) {
    for (size_t i = 0; i < PLAYER_COUNT_MAX; i++) {
        if (players[i].status != PS_EMPTY) {
            if (strcmp(players[i].username, username) == 0) {
                return true;
            }
        }
    }

    return false;
}

player *get_player_for_addr(int socket, struct sockaddr *addr, socklen_t addr_len) {
    for (size_t i = 0; i < PLAYER_COUNT_MAX; i++) {
        if (players[i].status != PS_EMPTY) {
            //TODO: ugly workaround, these structs may contain padding which may be set to anything
            //and this comparison will fail :-/
            if (players[i].socket == socket && players[i].addr_len == addr_len && memcmp(&players[i].addr, addr, addr_len) == 0) {
                return &players[i];
            }
        }
    }

    return NULL;
}