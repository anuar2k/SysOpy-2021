#pragma once

#define CHAT_PROJ_ID 69
#define CLIENTS_MAX 20
#define MESSAGE_MAX (100 + 1)

typedef enum {
    ST_EMPTY,
    ST_AVAIL,
    ST_BUSY
} client_status;

// messages sent to the client
typedef enum {
    C_MSG_STOP = 1,
    C_MSG_DISCONNECT,
    C_MSG_CONNECT,
    C_MSG_MESSAGE,
    C_MSG_LIST,
    C_MSG_INIT,
    C_MSG_TYPE_MAX = C_MSG_INIT
} c_msg_type;

typedef struct {
    int other_client_queue_id;
} c_msg_connect;

typedef struct {
    char message[MESSAGE_MAX];
} c_msg_message;

typedef struct {
    size_t clients_count;
    client_status clients[CLIENTS_MAX];
} c_msg_list;

typedef struct {
    unsigned char client_id;
} c_msg_init;

typedef union {
    c_msg_connect connect_p;
    c_msg_message message_p;
    c_msg_init       init_p;
    c_msg_list       list_p;
} c_msg;

typedef struct {
    long mtype;
    c_msg msg;
} c_msg_buf;

// messages sent to the server
typedef enum {
    S_MSG_STOP = 1,
    S_MSG_DISCONNECT,
    S_MSG_CONNECT,
    S_MSG_LIST,
    S_MSG_INIT,
    S_MSG_TYPE_MAX = S_MSG_INIT
} s_msg_type;

typedef struct {
    unsigned char client_id;
} s_msg_stop;

typedef struct {
    unsigned char client_id;
} s_msg_disconnect;

typedef struct {
    unsigned char client_id;
    unsigned char other_client_id;
} s_msg_connect;

typedef struct {
    int client_queue_id;
} s_msg_init;

typedef struct {
    unsigned char client_id;
} s_msg_list;

typedef union {
    s_msg_stop             stop_p;
    s_msg_disconnect disconnect_p;
    s_msg_connect       connect_p;
    s_msg_init             init_p;
    s_msg_list             list_p;
} s_msg;

typedef struct {
    long mtype;
    s_msg msg;
} s_msg_buf;
