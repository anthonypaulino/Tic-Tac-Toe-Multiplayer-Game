#ifndef _PROTOCOL_H
#define _PROTOCOL_H
#define BUFSIZE 264 


// [FOR GAME FUNCTION] (FREE & IN_USE are markers for player_t) 
typedef enum {X, O, no_win, draw, no_draw, FREE, IN_USE, invalid, valid, no_kind} kind_t; 
// [MSG FUNCTION]
typedef enum {PLAY, MOVE, RSGN, DRAW, BEGN, MOVD, INVL, OVER, WAIT} msg_t; 
// [invalid]
typedef enum {unexpected, user_taken, invalid_move, invalid_draw, winner, looser, no_winner, both_draw, resign, malformed, no_cause,capacity, dump} cause_t; 

typedef struct handle_t{
    int fd; 
    char buffer[BUFSIZE + 1];
    int b_count, start, real_len;
}handle_t;

//For our player list
typedef struct player_t{
    handle_t *handle;
    kind_t avail;
    char* name;
}player_t;

//Struct sent to a thread to recieve "PLAY" message from clients that's connection has been accepted
typedef struct pair_t{ 
    player_t *X;
    player_t *O;
}pair_t;

//Struct sent to a thread to run a game
typedef struct game_t{
    char board[3][3];
    pair_t *info;
    kind_t winner;
}game_t;

typedef struct msg_data{
    char *msg;
    msg_t type;
    char *third_field;
    char *fourth_field;
    char *fifth_field;
}msg_data;


int retIndexOfNextField(char* in, int current);
char* retNextField(char* in, int index);
void init_msg(handle_t *handle, int fd);
void write_serv_msg(int sockfd, char *make);
void write_cli_msg(int sockfd, char *make, msg_data *recv);

msg_data *get_msg(handle_t *handle);
void free_msg(msg_data *data);

player_t *init_player( unsigned capacity);
void dest_player(player_t *list);
int create_player(player_t *list,char *name);
int remove_player(player_t *list);
int get_count();
void print_p( player_t *list);


#endif