#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include "protocol.h"

#define DEBUG 0 // Debugging purposes
#define GAME 1 // game messages
#define CAPACITY 300 // player capacity
#define QUEUE_SIZE 8 // Queue size for listen()
#define BUFSIZE 264 // Buffer Size
#define HOSTSIZE 100 // HOST NAME
#define PORTSIZE 10 // PORT NAME


volatile int active = 1; //track if server is active
pair_t *pair_status;
player_t *players; // need to track all players username(only unique names)
//static int thread_count = 1;
pthread_mutex_t lock;



void handler(int signum)
{
    active = 0;
}

void install_handlers()
{
    struct sigaction act;
    act.sa_handler = handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}

int open_listener(char *service, int queue_size)
{
    struct addrinfo hint, *info_list, *info;
    int error, sockfd;
    // initialize hints
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;
    // obtain information for listening socket
    error = getaddrinfo(NULL, service, &hint, &info_list);
    if (error) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }
    // attempt to create socket
    for (info = info_list; info != NULL; info = info->ai_next) {
        sockfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        // if we could not create the socket, try the next method
        if (sockfd == -1) continue;
            // bind socket to requested port
            error = bind(sockfd, info->ai_addr, info->ai_addrlen);
            if (error) {
            close(sockfd);
            continue;
        }
            // enable listening for incoming connection requests
            error = listen(sockfd, queue_size);
            if (error) {
            close(sockfd);
            continue;
        }
            // if we got this far, we have opened the socket
            break;
    }
    freeaddrinfo(info_list);
    // info will be NULL if no method succeeded
    if (info == NULL) {
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    return sockfd;
}

int get_client(int listener_fd, char *service){
    struct sockaddr_storage remote_host;
    socklen_t remote_host_len;    
    char host[HOSTSIZE], port[PORTSIZE];
    int error, client_sockfd;
    
    listen(listener_fd, QUEUE_SIZE);

    if(DEBUG)puts("Listening for incoming connections");
    remote_host_len = sizeof(remote_host);
    memset(&remote_host, 0, remote_host_len);
    
    while(1){
        client_sockfd = accept(listener_fd, (struct sockaddr *)&remote_host, &remote_host_len);
        if (client_sockfd < 0){ 
            perror("\naccept"); 
            if(!active){ //signal caught, server shutting down
                return -1;
            } //signal caught, server shutting down
            continue;
        } 
        else break;
    }

    // obtain information for listening socket
    error = getnameinfo((struct sockaddr *)&remote_host, remote_host_len, host, HOSTSIZE, port, PORTSIZE,NI_NUMERICSERV);
    if (error) {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(error));
        strcpy(host, "??");
        strcpy(port, "??");
    }

    if(GAME)printf("Accepted connection from client [%s:%s]\n",host,port);

 
    return client_sockfd;
}


/* FIXME: writing, getting players move, check if valid move, update board, log/update of game, general check board function */

void init_board(game_t *in)
{
    for(int i = 0;i<3;i++){
        for(int j = 0;j<3;j++){
            in->board[i][j] = ' ';
        }
    }
}

void print_board(game_t * in)
{   
    char * x = (*in->info->X).name;
    char * o = (*in->info->O).name;
    printf("grid state of game with player %s (X) and player %s (O)\n",x,o);
    printf(" %c | %c | %c \n", in->board[0][0], in->board[0][1], in->board[0][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", in->board[1][0], in->board[1][1], in->board[1][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", in->board[2][0], in->board[2][1], in->board[2][2]);
}

char *make_grid(game_t * in){
    char *grid = malloc(sizeof(char)*10);
    grid[0] = in->board[0][0];
    grid[1] = in->board[0][1];
    grid[2] = in->board[0][2];
    grid[3] = in->board[1][0];
    grid[4] = in->board[1][1];
    grid[5] = in->board[1][2];
    grid[6] = in->board[2][0];
    grid[7] = in->board[2][1];
    grid[8] = in->board[2][2];

    for(int i=0;i<9;++i){if( isspace(grid[i]) ) grid[i] = '.';}
    grid[9] = '\0';
    return grid;
}

static kind_t checkMove(char *move, game_t *in, kind_t player){
    // move is always in the format ([1-3],[1-3])
    int x = (move[0] - '0') -1; // first character
    int y = (move[2] - '0') -1;// third character
    if(isspace(in->board[x][y])){
        if(player == X) in->board[x][y] = 'X';
        else if(player == O) in->board[x][y] = 'O';
        return valid;
    }
    else return invalid;
}

static kind_t checkWinState(game_t *in)
{
    //check for completed columns and/or rows->
    for(int i = 0;i<3;i++){
        if(!isspace(in->board[i][0]) && in->board[i][0]==in->board[i][1] && in->board[i][0]==in->board[i][2]){
            //if char is an 'x', thats the winner return X. Otherwise the winner is 'o' return O
            if(DEBUG)puts("Win by row.");
            return (in->board[i][0] == 'X' ? X : O);
        }
        if(!isspace(in->board[0][i]) && in->board[0][i]==in->board[1][i] && in->board[0][i]==in->board[2][i]){
            if(DEBUG)puts("Win by col.");
            return (in->board[0][i] == 'X' ? X : O);
        }
    }
    //now the diagonals->
    if(!isspace(in->board[0][0]) && in->board[0][0]==in->board[1][1] && in->board[0][0]==in->board[2][2]){
        if(DEBUG)puts("Win by diagnol.");
        return (in->board[1][1] == 'X' ? X : O);
        }
    
    if(!isspace(in->board[0][2]) && in->board[0][2]==in->board[1][1] && in->board[0][2]==in->board[2][0]){
        if(DEBUG)puts("Win by diagnol.");
        return (in->board[1][1] == 'X' ? X : O);
    }

    return no_win;
}

static kind_t checkDraw(game_t *in)
{
    for(int i = 0;i<3;i++){
        for(int j = 0;j<3;j++){
            if(isspace(in->board[i][j]))
                return no_draw;
        }
    }
    return draw;
}
//need to make sure message received is expected
char *make_message(game_t *game, msg_t msg_type, msg_data * msg, kind_t role, cause_t cause){
    char *tempMessage;
    char otherMessage[BUFSIZE+1];
    int state = 0;
    //need to add the conditions of roles, and unexpected 
    switch(msg_type){
        case WAIT:
            //server recieves PLAY
            tempMessage = "WAIT|0|";
            break;
        case BEGN: //server initiates BEGN after starting game
            memmove(otherMessage,"BEGN|",5);
            if(role==X){ 
                char int_str[4]; //integer as a string
                int n = strlen((*game->info->O).name); //length of string
                n=n+3; // 2 bytes for X|, 1 byte for last vertical bar
                sprintf(int_str, "%d",n); // store integer in string 
                memmove(otherMessage+5,int_str,strlen(int_str));
                memmove(otherMessage+5+strlen(int_str),"|",1);
                memmove(otherMessage+6+strlen(int_str),"X|",2);
                memmove(otherMessage+8+strlen(int_str),((*game->info->O).name),(n=n-3));
                memmove(otherMessage+8+strlen(int_str)+n,"|",1);
                otherMessage[9+strlen(int_str)+n] = '\0';
            }
            else if(role==O){
                char int_str[4]; //integer as a string
                int n = strlen((*game->info->X).name); //length of string
                n=n+3; // 2 bytes for X|, 1 byte for last vertical bar
                sprintf(int_str, "%d",n); // store integer in string 
                memmove(otherMessage+5,int_str,strlen(int_str));
                memmove(otherMessage+5+strlen(int_str),"|",1);
                memmove(otherMessage+6+strlen(int_str),"O|",2);
                memmove(otherMessage+8+strlen(int_str),((*game->info->X).name),(n=n-3));
                memmove(otherMessage+8+strlen(int_str)+n,"|",1);
                otherMessage[9+strlen(int_str)+n] = '\0';
            }
            state=1;
            break;
        case MOVD: //servers recieves MOVE
            memmove(otherMessage,"MOVD|",5);
            int x = strlen((msg->fourth_field));
            char *grid = make_grid(game);
            if(role==X){ // 2 bytes for X|, 3 bytes for position, 9 bytes for the grid
                memmove(otherMessage+5,"16|X|",5);
                memmove(otherMessage+10,((msg->fourth_field)),x);
                memmove(otherMessage+10+x,"|",1);
                memmove(otherMessage+11+x,grid,9);
                memmove(otherMessage+20+x,"|",1);
                otherMessage[21+x] = '\0';
            }
            else if(role==O){
                memmove(otherMessage+5,"16|O|",5);
                memmove(otherMessage+10,((msg->fourth_field)),x);
                memmove(otherMessage+10+x,"|",1);
                memmove(otherMessage+11+x,grid,9);
                memmove(otherMessage+20+x,"|",1);
                otherMessage[21+x] = '\0';
            }
            free(grid);
            state=1;
            break;
        case INVL: 
            if(cause==unexpected) tempMessage = "INVL|51|!received an unexpected message, please try again.|";// unexpected message
            else if(cause==user_taken) tempMessage = "INVL|39|!name is occupied, choose another one.|";// username taken
            else if(cause==invalid_move) tempMessage = "INVL|45|!this move is not allowed, please try again.|";// invalid move
            else if(cause==malformed) tempMessage = "INVL|61|!Malformed message received from client, closing connection.|";// malformed
            else if(cause==capacity) tempMessage = "INVL|99|!player capacity has been reached, increase \"CAPACITY\" macro for more players, closing connection.|";
            else if(cause==dump) tempMessage = "INVL|36|!invalid message, please try again.|";
            break;
        case DRAW: //server recieves draw
            if(strcmp((msg->third_field),"S")==0) tempMessage = "DRAW|2|S|";
            else if(strcmp((msg->third_field),"A")==0) tempMessage = "DRAW|2|A|"; 
            else if(strcmp((msg->third_field),"R")==0) tempMessage = "DRAW|2|R|";
            break;
        case OVER: //winner, no winner, agree to draw, resign 
     
            memmove(otherMessage,"OVER|",5);
            int a = strlen((*game->info->X).name);
            int b = strlen((*game->info->O).name);
            a=a+16+2; b=b+16+2;
            char int_x_str[4];
            char int_o_str[4];
            sprintf(int_x_str, "%d",a); 
            sprintf(int_o_str, "%d",b); 
            if(cause==winner){
                if(game->winner == X){
                    memmove(otherMessage+5,int_x_str,strlen(int_x_str));
                    memmove(otherMessage+5+strlen(int_x_str),"|",1);
                    memmove(otherMessage+6+strlen(int_x_str),"W|",2);
                    memmove(otherMessage+8+strlen(int_x_str),((*game->info->X).name),(a=a-16-2));
                    memmove(otherMessage+8+strlen(int_x_str)+a," is the winner.|",16);
                    otherMessage[24+strlen(int_x_str)+a] = '\0';
                }
                else{
                    memmove(otherMessage+5,int_o_str,strlen(int_x_str));
                    memmove(otherMessage+5+strlen(int_o_str),"|",1);
                    memmove(otherMessage+6+strlen(int_o_str),"W|",2);
                    memmove(otherMessage+8+strlen(int_o_str),((*game->info->O).name),(b=b-16-2));
                    memmove(otherMessage+8+strlen(int_o_str)+b," is the winner.|",16);
                    otherMessage[24+strlen(int_x_str)+b] = '\0';
                } 
                state=1;
            }
            else if(cause==looser){
                if(game->winner == X){
                    memmove(otherMessage+5,int_x_str,strlen(int_x_str));
                    memmove(otherMessage+5+strlen(int_x_str),"|",1);
                    memmove(otherMessage+6+strlen(int_x_str),"L|",2);
                    memmove(otherMessage+8+strlen(int_x_str),((*game->info->X).name),(a=a-16-2));
                    memmove(otherMessage+8+strlen(int_x_str)+a," is the winner.|",16);
                    otherMessage[24+strlen(int_x_str)+a] = '\0';
                }
                else{
                    memmove(otherMessage+5,int_o_str,strlen(int_x_str));
                    memmove(otherMessage+5+strlen(int_o_str),"|",1);
                    memmove(otherMessage+6+strlen(int_o_str),"L|",2);
                    memmove(otherMessage+8+strlen(int_o_str),((*game->info->O).name),(b=b-16-2));
                    memmove(otherMessage+8+strlen(int_o_str)+b," is the winner.|",16);
                    otherMessage[24+strlen(int_x_str)+b] = '\0';
                }
                state=1;
            }
            else if(cause==no_winner) tempMessage = "OVER|36|D|board filled up without a winner.|"; //both
            else if(cause==both_draw) tempMessage = "OVER|36|D|both players have agreed to draw.|";
            else if(cause==resign){
                memmove(otherMessage,"OVER|",5);
                int c = strlen((*game->info->X).name);
                int d = strlen((*game->info->O).name);
                c=c+11+2; d=d+11+2;
                char int_c_str[4];
                char int_d_str[4];
                sprintf(int_c_str, "%d",c); 
                sprintf(int_d_str, "%d",d); 
                if(game->winner==X && role==X){ 
                    memmove(otherMessage+5,int_d_str,strlen(int_d_str));
                    memmove(otherMessage+5+strlen(int_d_str),"|",1);
                    memmove(otherMessage+6+strlen(int_d_str),"W|",2);
                    memmove(otherMessage+8+strlen(int_d_str),((*game->info->O).name),(d=d-11-2));
                    memmove(otherMessage+8+strlen(int_d_str)+d," resigned.|",11);
                    otherMessage[20+strlen(int_x_str)+d] = '\0';
                }
                else if(game->winner == X && role==O) {
                    memmove(otherMessage+5,int_d_str,strlen(int_d_str));
                    memmove(otherMessage+5+strlen(int_d_str),"|",1);
                    memmove(otherMessage+6+strlen(int_d_str),"L|",2);
                    memmove(otherMessage+8+strlen(int_d_str),((*game->info->O).name),(d=d-11-2));
                    memmove(otherMessage+8+strlen(int_d_str)+d," resigned.|",11);
                    otherMessage[20+strlen(int_x_str)+d] = '\0'; 
                }
                if(game->winner==O && role==O){
                    memmove(otherMessage+5,int_c_str,strlen(int_c_str));
                    memmove(otherMessage+5+strlen(int_c_str),"|",1);
                    memmove(otherMessage+6+strlen(int_c_str),"W|",2);
                    memmove(otherMessage+8+strlen(int_c_str),((*game->info->X).name),(c=c-11-2));
                    memmove(otherMessage+8+strlen(int_c_str)+c," resigned.|",11);
                    otherMessage[20+strlen(int_x_str)+c] = '\0';
                }
                else if(game->winner==O && role == X) {
                    memmove(otherMessage+5,int_c_str,strlen(int_c_str));
                    memmove(otherMessage+5+strlen(int_c_str),"|",1);
                    memmove(otherMessage+6+strlen(int_c_str),"L|",2);
                    memmove(otherMessage+8+strlen(int_c_str),((*game->info->X).name),(c=c-11-2));
                    memmove(otherMessage+8+strlen(int_c_str)+c," resigned.|",11);
                    otherMessage[20+strlen(int_x_str)+c] = '\0';
                }
                state=1;
            
            }
            break;
        case PLAY: 
        break; 
        case MOVE: 
        break; 
        case RSGN: 
        break; 
    }
    char *finalMessage;
    if(state){
       finalMessage =  malloc(strlen(otherMessage)+1);
       strcpy(finalMessage,otherMessage);
    }
    else{
        finalMessage =  malloc(strlen(tempMessage)+1);
        strcpy(finalMessage,tempMessage);
    }
    return finalMessage;
}

void *run_game(void *data)
{   
    /* Game Setup*/
    game_t *game = (game_t*)data; 
    player_t x = (*game->info->X);
    player_t o = (*game->info->O);
    handle_t * x_handle = game->info->X->handle;
    handle_t * o_handle = game->info->O->handle;
    int x_fd = game->info->X->handle->fd;
    int o_fd = game->info->O->handle->fd;
    init_board(game);   

    if(GAME)printf("a game started with player X:%s & player O:%s!\n",x.name,o.name);
    char *BEGN_X = make_message(game,BEGN,NULL,X,no_cause);
    char *BEGN_O = make_message(game,BEGN,NULL,O,no_cause);

    if(GAME)printf("---> [%s] <---\n---> [%s] <---\n",BEGN_X,BEGN_O);
    write_cli_msg(x_fd,BEGN_X,NULL);
    write_cli_msg(o_fd,BEGN_O,NULL);
    
    kind_t player_turn = X;
    int game_over = 0;
    int draw_sugest = 0;
    int error = 0; 
    

    while( active && !(game_over) ){

        msg_data *game_msg;
        int valid = 0;
        
        //check valid move
        while(active && !valid) {

            if (player_turn == X) game_msg = get_msg(x_handle);
            else game_msg = get_msg(o_handle);
            //msg not malformed
            if(game_msg!=NULL){
                //invalid message
                if(strcmp(game_msg->msg,"dump")== 0){
                char *makeMessage = make_message(game,INVL,game_msg,player_turn,dump);
                if(GAME)printf("---> [%s] <---\n", makeMessage);
                if(player_turn == X) write_cli_msg(x_fd,makeMessage,game_msg);
                else write_cli_msg(o_fd,makeMessage,game_msg);
                continue;
                }
                //unexecpted message not (MOVE,RSGN,DRAW)
                if(!(game_msg->type == MOVE || game_msg->type == RSGN || game_msg->type == DRAW)){
                    char *makeMessage = make_message(game,INVL,game_msg,player_turn,unexpected);
                    if(GAME)printf("---> [%s] <---\n", makeMessage);
                    if(player_turn == X) write_cli_msg(x_fd,makeMessage,game_msg);
                    else write_cli_msg(o_fd,makeMessage,game_msg);
                    continue;
                }
                //if draw S, must reply with draw A, R only
                if(draw_sugest && !(game_msg->type == DRAW)){
                    char *makeMessage = make_message(game,INVL,game_msg,player_turn,invalid_move);
                        if(GAME)printf("---> [%s] <---\n", makeMessage);
                        if(player_turn == X) write_cli_msg(x_fd,makeMessage,game_msg);
                        else write_cli_msg(o_fd,makeMessage,game_msg);
                        continue;
                }

                //message is either MOVE, RSGN, DRAW
                else if(game_msg->type == RSGN){
                        if(player_turn == X) game->winner = O;
                        else game->winner = X;
                        char *RSGN_X = make_message(game,OVER,game_msg,X,resign);
                        char *RSGN_O = make_message(game,OVER,game_msg,O,resign);
                        if(GAME)printf("---> [%s] <---\n---> [%s] <---\n",RSGN_X,RSGN_O);
                        write_cli_msg(x_fd,RSGN_X,game_msg);
                        write_cli_msg(o_fd,RSGN_O,NULL);
                        valid = 1;
                        game_over = 1;

                }

                else if(game_msg->type == DRAW){
                    //no draw S, but move is draw A/R
                    if(!draw_sugest && !(strcmp((game_msg->third_field),"S")==0) ){
                        char *makeMessage = make_message(game,INVL,game_msg,player_turn,invalid_move);
                        if(GAME)printf("---> [%s] <---\n", makeMessage);
                        if(player_turn == X) write_cli_msg(x_fd,makeMessage,game_msg);
                        else write_cli_msg(o_fd,makeMessage,game_msg);
                        continue;
                    }
                    //no draw S, but move is draw S
                    else if(!draw_sugest && (strcmp((game_msg->third_field),"S")==0)){
                        draw_sugest = 1;
                        //send draw suggestion
                        char *makeMessage = make_message(game,DRAW,game_msg,player_turn,no_cause);
                        if(GAME)printf("---> [%s] <---\n", makeMessage);
                        if(player_turn == X) write_cli_msg(o_fd,makeMessage,game_msg);
                        else write_cli_msg(x_fd,makeMessage,game_msg);
                        valid = 1;
                    }
                    //draw S, but move is draw S
                    else if(draw_sugest && !(strcmp((game_msg->third_field),"A")==0 || strcmp((game_msg->third_field),"R")==0)){
                        char *makeMessage = make_message(game,INVL,game_msg,player_turn,invalid_move);
                        if(GAME)printf("---> [%s] <---\n", makeMessage);
                        if(player_turn == X) write_cli_msg(x_fd,makeMessage,game_msg);
                        else write_cli_msg(o_fd,makeMessage,game_msg);
                    }
                    //draw S, but move is draw A/R
                    else if(draw_sugest && (strcmp((game_msg->third_field),"A")==0 || strcmp((game_msg->third_field),"R")==0)){
                        if(strcmp((game_msg->third_field),"A")==0){
                            char *makeMessage1 = make_message(game,OVER,game_msg,player_turn,both_draw);
                            char *makeMessage2 = make_message(game,OVER,game_msg,player_turn,both_draw);
                            if(GAME)printf("---> [%s] <---\n", makeMessage1);
                            //write to both that game is over, draw accepted
                            write_cli_msg(x_fd,makeMessage1,game_msg);
                            write_cli_msg(o_fd,makeMessage2,NULL);
                            valid=1;
                            game_over=1;
                        }
                        else{
                        draw_sugest = 0;
                        char *makeMessage = make_message(game,DRAW,game_msg,player_turn,no_cause);
                        if(GAME)printf("---> [%s] <---\n", makeMessage);
                        if(player_turn == X) write_cli_msg(o_fd,makeMessage,game_msg);
                        else write_cli_msg(x_fd,makeMessage,game_msg);
                        valid = 1;
                        }
                        
                    }
                }
                else if(game_msg->type == MOVE){
                    kind_t ret = checkMove(game_msg->fourth_field, game, player_turn);
                    if(ret == invalid){
                        char *makeMessage = make_message(game,INVL,game_msg,player_turn,invalid_move);
                        if(GAME)printf("---> [%s] <---\n", makeMessage);
                        if(player_turn == X) write_cli_msg(x_fd,makeMessage,game_msg);
                        else write_cli_msg(o_fd,makeMessage,game_msg);
                        continue;
                    }
                    else{
                        //move is valid, send to both clients
                        char *makeMessage1 = make_message(game,MOVD,game_msg,player_turn,no_cause);
                        char *makeMessage2 = make_message(game,MOVD,game_msg,player_turn,no_cause);
                        if(GAME)printf("---> [%s] <---\n", makeMessage1);
                        write_cli_msg(x_fd,makeMessage1,game_msg);
                        write_cli_msg(o_fd,makeMessage2,NULL);
                        valid=1;
                    }

                }
            }
            //msg malformed
            else if(game_msg==NULL){
                char *makeMessage1 = make_message(NULL,INVL,NULL,player_turn,malformed);
                char *makeMessage2 = make_message(NULL,INVL,NULL,player_turn,malformed);
                if(GAME)printf("---> [%s] <---\n", makeMessage1);
                write_cli_msg(x_fd,makeMessage1,NULL);
                write_cli_msg(o_fd,makeMessage2,NULL);
                valid = 1;
                game_over = 1;
                error = 1;
            }    
        }
        if(error)break; 
        //game stopping due to draw or winner
        kind_t c_winner = checkWinState(game);
        if(c_winner != no_win) {
            game->winner = c_winner;
            char *game_winner = make_message(game,OVER,NULL,player_turn,winner);
            char *game_looser = make_message(game,OVER,NULL,player_turn,looser);
            if(GAME)printf("---> [%s] <---\n---> [%s] <---\n",game_winner,game_looser);
            if(c_winner == X){
                write_cli_msg(x_fd,game_winner,NULL);
                write_cli_msg(o_fd,game_looser,NULL);
            }
            else{
                write_cli_msg(o_fd,game_winner,NULL);
                write_cli_msg(x_fd,game_looser,NULL); 
            }
            game_over=1;
        }
        kind_t c_draw = checkDraw(game);
        if(c_draw == draw){
            char *makeMessage1 = make_message(game,OVER,NULL,player_turn,no_winner);
            char *makeMessage2 = make_message(game,OVER,NULL,player_turn,no_winner);
            if(GAME)printf("---> [%s] <---\n", makeMessage1);
            write_cli_msg(x_fd,makeMessage1,NULL);
            write_cli_msg(o_fd,makeMessage2,NULL);
            game_over=1;
        }
        //change who's turn it is
        if(player_turn == X) player_turn = O;
        else player_turn = X;
        print_board(game);
    }


    if(GAME)printf("Game with player X:%s & Player O:%s has ended!\n",x.name,o.name);

    close(x_fd);
    close(o_fd);
    pthread_mutex_lock(&lock);
    remove_player(game->info->X);
    remove_player(game->info->O);
    pthread_mutex_unlock(&lock);
    free(x_handle);
    free(o_handle);
    free(game->info);
    free(game);
    pthread_exit(NULL);
}

void *pair_clients(void *data){

    handle_t *handle = (handle_t *)data; 
    //Get PLAY from client 
    msg_data *play_msg;
    while( ((play_msg = get_msg(handle)) != NULL) && active){
        
        if(strcmp(play_msg->msg,"dump")== 0){
            char *makeMessage = make_message(NULL,INVL,play_msg,no_kind,dump);
            if(GAME)printf("---> [%s] <---\n", makeMessage);
            write_cli_msg(handle->fd,makeMessage,play_msg);
            continue;
        }
        else if(play_msg->type != PLAY){
            char *makeMessage = make_message(NULL,INVL,play_msg,no_kind,unexpected);
            if(GAME)printf("---> [%s] <---\n", makeMessage);
            write_cli_msg(handle->fd,makeMessage,play_msg);
            continue;
        }
        else if(play_msg->type == PLAY){
            pthread_mutex_lock(&lock);
            //First player
            if(pair_status->X == NULL){ 
                int id = create_player(players,play_msg->third_field);
                if(id == -1){ 
                    pthread_mutex_unlock(&lock);
                    char *makeMessage = make_message(NULL,INVL,play_msg,X,user_taken);
                    if(GAME)printf("---> [%s] <---\n", makeMessage);
                    write_cli_msg(handle->fd,makeMessage,play_msg);
                    continue;
                }
                else if(id == -2){
                    pthread_mutex_unlock(&lock);
                    char *makeMessage = make_message(NULL,INVL,play_msg,X,capacity);
                    if(GAME)printf("---> [%s] <---\n", makeMessage);
                    write_cli_msg(handle->fd,makeMessage,play_msg);
                    free(handle);
                    break;
                }
                else{
                    players[id].handle=handle;
                    pair_status->X = &(players[id]);
                    pthread_mutex_unlock(&lock);
                    char *makeMessage = make_message(NULL,WAIT,play_msg,X,no_cause);
                    if(GAME)printf("---> [%s] <---\n", makeMessage);
                    write_cli_msg(handle->fd,makeMessage,play_msg);
                    break;
                }
            } 
            //Second player
            else if(pair_status->O ==NULL){
                int id = create_player(players,play_msg->third_field);
                if(id == -1){ 
                    pthread_mutex_unlock(&lock);
                    char *makeMessage = make_message(NULL,INVL,play_msg,O,user_taken);
                    if(GAME)printf("---> [%s] <---\n", makeMessage);
                    write_cli_msg(handle->fd,makeMessage,play_msg);
                    continue;
                }
                else if(id == -2){
                    pthread_mutex_unlock(&lock);
                    char *makeMessage = make_message(NULL,INVL,play_msg,O,capacity);
                    if(GAME)printf("---> [%s] <---\n", makeMessage);
                    write_cli_msg(handle->fd,makeMessage,play_msg);
                    free(handle);
                    break;
                }
                else{
                    players[id].handle=handle;
                    char *makeMessage = make_message(NULL,WAIT,play_msg,O,no_cause);
                    if(GAME)printf("---> [%s] <---\n", makeMessage);
                    write_cli_msg(handle->fd,makeMessage,play_msg);
                    pthread_t game_thread; // game thread 
                    pair_t *data = (pair_t*)malloc(sizeof(pair_t)*1); // making temp game info so we can re-use global pair_t variable
                    data->X = pair_status->X; data->O = &(players[id]);
                    pair_status->X = NULL; 
                    game_t *game = (game_t*)malloc(sizeof(game_t)*1); game->info = data;
                    pthread_mutex_unlock(&lock);
                    int ret = pthread_create(&game_thread, NULL, run_game, (void *)game); // get PLAY from clients through a thread
                    if (ret){ printf("Thread creation failed."); exit(EXIT_FAILURE);}   
                    if(DEBUG)puts("Game created with game thread.");
                    break;      
                }
            }
        }      
    }
    if(play_msg == NULL){
        char *makeMessage = make_message(NULL,INVL,play_msg,no_kind,malformed);
        if(GAME)printf("---> [%s] <---\n", makeMessage);
        write_cli_msg(handle->fd,makeMessage,play_msg);
        free(handle);
    }
    if(DEBUG)puts("exiting the thread to pair players.");
    pthread_exit(NULL);
}



int main(int argc, char *argv[])
{
   
    if (argc < 2) {
        puts("ERROR: no port provided");
        exit(EXIT_FAILURE);
    }

    char *service = argv[1]; //port number
    pair_status = (pair_t*)malloc(sizeof(pair_t)*1); //thread uses this indicate whether players are ready or not
    pair_status->X = NULL; pair_status->O =NULL; 
    install_handlers(); // for signal, ^C to shut down the server
    pthread_mutex_init(&lock, NULL); //allows to deal with global variables without the interference with other threads

    players = init_player(CAPACITY); //initialize player list

    int listener_fd = open_listener(service, QUEUE_SIZE); // listen to incoming connections
    if (listener_fd < 0) exit(EXIT_FAILURE);
    puts("Server is up and running.");
    /*get clients, than run a game through  a new created thread*/
    while (active) {
        
        handle_t *handle = malloc(sizeof(handle_t));
        int fd = get_client(listener_fd, service); 
        if(fd == -1) {free(handle); continue;}
        init_msg(handle,fd);

        if(DEBUG)printf("Player:%d is ready to get paired\n",handle->fd);

        pthread_t thread;
        int ret = pthread_create(&thread, NULL, pair_clients, (void *)handle); // get PLAY from clients through a thread
        if (ret){ printf("Thread creation failed."); exit(EXIT_FAILURE);}

    }

    puts("Server is shutting down!");
    //while(thread_count != 1)//waiting until thread count hits 1
    if(pair_status->X != NULL) free(pair_status->X->handle);
    if(pair_status->O != NULL) free(pair_status->O->handle);
    free(pair_status);
    dest_player(players);
    pthread_exit(NULL);

    return EXIT_SUCCESS;
}