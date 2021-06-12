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
    PS_RESERVED,
    PS_QUEUED,
    PS_PLAYING
} player_status;

struct player {
    player_status status;
    int socket;
    bool alive;
    char username[USERNAME_LEN_MAX];
    struct player *opponent;
    game *game;
};
typedef struct player player;

typedef enum {
    EP_SOCKET, EP_PLAYER
} epoll_payload_tag;

typedef struct {
    epoll_payload_tag tag;
    union {
        int socket;
        player *p;
    };
} epoll_payload;

int epoll;

player players[PLAYER_COUNT_MAX] = { 
    [0 ... PLAYER_COUNT_MAX - 1] = {
        .status = PS_EMPTY
    } 
};
pthread_mutex_t players_mut = PTHREAD_MUTEX_INITIALIZER;

void enable_incoming(int epoll, int socket, void *addr, size_t addr_len);
player *find_free_player_spot(void);
player *create_player(int socket);
player *get_queued_player(void);
void handle_player(player *p);
void delete_player(player *p);
void *pinger_thread(void *arg);
bool check_win(char board[9], char last_player);
bool check_tie(char board[9]);
bool username_used(const char *username);

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
    hints.ai_socktype = SOCK_STREAM;

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
    int unix_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unix_socket == -1) {
        perror("unix_socket");
        return EXIT_FAILURE;
    }
    int interwebz_socket = socket(AF_INET, SOCK_STREAM, 0);
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
        
        epoll_payload *payload = event.data.ptr;
        if (payload->tag == EP_SOCKET) {
            int socket = payload->socket;
            int client_socket = accept4(socket, NULL, NULL, SOCK_NONBLOCK);
            pthread_mutex_lock(&players_mut);
            player *p = create_player(client_socket);
            pthread_mutex_unlock(&players_mut);

            if (p == NULL) {
                static const packet_header full = PH_FULL;
                write(client_socket, &full, sizeof(full));
                close(client_socket);
            }
            else {
                epoll_payload *payload = malloc(sizeof(*payload));
                payload->tag = EP_PLAYER;
                payload->p = p;
                
                struct epoll_event event = {
                    .events = EPOLLIN | EPOLLHUP | EPOLLRDHUP,
                    .data.ptr = payload
                };

                if (epoll_ctl(epoll, EPOLL_CTL_ADD, client_socket, &event) != 0) {
                    perror("epoll_ctl");
                    return EXIT_FAILURE;
                }
            }
        }
        else {
            if (event.events & EPOLLRDHUP || event.events & EPOLLHUP) {
                pthread_mutex_lock(&players_mut);
                delete_player(payload->p);
                pthread_mutex_unlock(&players_mut);
            }
            else if (event.events & EPOLLIN) {
                pthread_mutex_lock(&players_mut);
                handle_player(payload->p);
                pthread_mutex_unlock(&players_mut);
            }
        }
    }
}

void enable_incoming(int epoll, int socket, void *addr, size_t addr_len) {
    if (bind(socket, addr, addr_len) != 0) {
        perror("bind");
        exit(1);
    }
    if (listen(socket, BACKLOG_MAX) != 0) {
        perror("listen");
        exit(1);
    }

    epoll_payload *payload = malloc(sizeof(*payload));
    payload->tag = EP_SOCKET;
    payload->socket = socket;
    struct epoll_event event = {
        .events = EPOLLIN | EPOLLPRI,
        .data.ptr = payload
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

player *create_player(int socket) {
    player *p = find_free_player_spot();
    if (p == NULL) {
        return NULL;
    }

    p->status = PS_RESERVED;
    p->socket = socket;
    p->alive = true;
    p->username[0] = '\0';
    p->opponent = NULL;
    p->game = NULL;
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

void handle_player(player *p) {
    int socket = p->socket;
    while (true) {
        packet_header header;
        int res = read(socket, &header, sizeof(header));
        if (res == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("handle_player read");
            }
            break;
        }
        switch (header) {
            case PH_HELLO: {
                char new_username[USERNAME_LEN_MAX];
                read(socket, new_username, USERNAME_LEN_MAX);
                if (username_used(new_username)) {
                    static const packet_header bad_uname = PH_BAD_UNAME;
                    write(socket, &bad_uname, sizeof(bad_uname));
                    break;
                }
                strcpy(p->username, new_username);

                player *possible_opponent = get_queued_player();
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
                        write(socket, &begin_x, sizeof(begin_x));
                        write(possible_opponent->socket, &begin_o, sizeof(begin_o));
                    }
                    else {
                        write(socket, &begin_o, sizeof(begin_o));
                        write(possible_opponent->socket, &begin_x, sizeof(begin_x));
                    }
                }
                else {
                    p->status = PS_QUEUED;
                }
            } break;
            case PH_MOVE: {
                unsigned char move;
                read(socket, &move, sizeof(move));

                char last_player = p->game->x_next ? 'X' : 'O';
                p->game->board[move] = last_player;

                if (check_win(p->game->board, last_player)) {
                    static const packet_header win = PH_WIN;
                    static const packet_header lose = PH_LOSE;

                    write(socket, &win, sizeof(win));
                    write(p->opponent->socket, &lose, sizeof(lose));
                }
                else if (check_tie(p->game->board)) {
                    static const packet_header tie = PH_TIE;
                    
                    write(socket, &tie, sizeof(tie));
                    write(p->opponent->socket, &tie, sizeof(tie));
                }
                else {
                    p->game->x_next = !p->game->x_next;

                    //poor man's packed struct...
                    static const size_t buf_size = sizeof(packet_header) + sizeof(game);
                    char buf[buf_size];
                    
                    *((packet_header*)&buf[0]) = PH_MOVE;
                    *((game*)&buf[sizeof(packet_header)]) = *p->game;

                    write(p->opponent->socket, &buf, buf_size);
                }
            } break;
            case PH_PING: {
                p->alive = true;
                printf("received ping\n");
            } break;
            default: {
                delete_player(p);
            } break;
        }
    }
}

void delete_player(player *p) {
    if (p->status == PS_PLAYING && p->opponent != NULL) {
        static const packet_header opp_dc = PH_OPP_DC;
        free(p->game);
        p->opponent->game = NULL;
        p->opponent->opponent = NULL;
        write(p->opponent->socket, &opp_dc, sizeof(opp_dc));
    }

    epoll_ctl(epoll, EPOLL_CTL_DEL, p->socket, NULL);
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
                    write(p->socket, &ping, sizeof(ping));
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