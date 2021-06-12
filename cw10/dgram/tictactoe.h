#define USERNAME_LEN_MAX (20 + 1)
#define PING_KEEPALIVE 5

typedef struct {
    char board[9];
    bool x_next;
} game;

typedef enum {
    PH_HELLO,     //to server with username
    PH_FULL,      //to client when full
    PH_BAD_UNAME, //to client
    PH_BEGIN_X,   //to client
    PH_BEGIN_O,   //to client
    PH_MOVE,      //to client with game, to server with field
    PH_WIN,       //to client
    PH_LOSE,      //to client
    PH_TIE,       //to client
    PH_PING,      //to client, to server
    PH_OPP_DC     //to client
} packet_header;
