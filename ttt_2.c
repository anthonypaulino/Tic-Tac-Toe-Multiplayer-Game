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
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include "protocol.h"

#define DEBUG 1 // Debugging purposes
volatile int active = 1; 
kind_t sign = -1;
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
    sigaction(SIGPIPE, &act, NULL);
}

int connect_inet(char *host, char *service)
{
    struct addrinfo hints, *info_list, *info;
    int sock, error;
    // look up remote host
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // in practice, this means give us IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // indicate we want a streaming socket
    error = getaddrinfo(host, service, &hints, &info_list);
    if (error) {
    fprintf(stderr, "error looking up %s:%s: %s\n", host, service,
    gai_strerror(error));
    return -1;
    }
    for (info = info_list; info != NULL; info = info->ai_next) {
        sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (sock < 0) continue;
            error = connect(sock, info->ai_addr, info->ai_addrlen);
        if (error) {
            close(sock);
            continue;
        }
        break;
    }
    freeaddrinfo(info_list);
    if (info == NULL) {
        fprintf(stderr, "Unable to connect to %s:%s\n", host, service);
        return -1;
    }
    return sock;
}

int findNumDigits(int in)
{
    int i = 0;
    while(in>0){
        in/=10;
        i++;
    }
    return i;
}


char* makeMessage(char* buf){
    char* command = retNextField(buf,0);

    if(strcmp(command,"PLAY")==0){
        int nextIndex = retIndexOfNextField(buf,0);
        if(nextIndex==-1||nextIndex>(strlen(buf)-3))
            return "INVL";
        
        command = realloc(command, strlen(command)+2);
        strcat(command,"|");

        char* name = retNextField(buf,nextIndex);
        name = realloc(name,strlen(name)+2);
        strcat(name,"|");

        int length = strlen(name);
        char* secondField = malloc(sizeof(char)*(findNumDigits(length)+2));
        sprintf(secondField,"%d|",length);

        command = realloc(command,(strlen(command)+strlen(secondField)+1));
        strcat(command,secondField);
        command = realloc(command,(strlen(command)+strlen(name)+1));
        strcat(command,name);

        free(name);
        free(secondField);
        return command;
    }

    else if(strcmp(command,"MOVE")==0){
        int nextIndex = retIndexOfNextField(buf,0);
        if(nextIndex==-1||nextIndex>(strlen(buf)-3)){
            return "INVL";
        }
        command = realloc(command, strlen(command)+2);
        strcat(command,"|");

        
        char* row = retNextField(buf, nextIndex);
        if(strcmp(row,"1")!=0&&strcmp(row,"2")!=0&&strcmp(row,"3")!=0)      //row can only be 1, 2, or 3.
            return "INVL";
        row = realloc(row,strlen(row)+2);
        strcat(row,",");

        nextIndex = retIndexOfNextField(buf, nextIndex);
        char* col = retNextField(buf, nextIndex);
        if(strcmp(col,"1")!=0&&strcmp(col,"2")!=0&&strcmp(col,"3")!=0)      //row can only be 1, 2, or 3.
            return "INVL";
        col = realloc(col,strlen(col)+2);
        strcat(col,"|");

        int length = 6;
        char* secondField = malloc(sizeof(char)*(findNumDigits(length)+2));
        sprintf(secondField,"%d|",length);

        command = realloc(command,(strlen(command)+strlen(secondField)+1));
        strcat(command,secondField);
        
        if(sign==X){
            command = realloc(command, strlen(command)+3);
            strcat(command,"X|");
        }
        if(sign==O){
            command = realloc(command, strlen(command)+3);
            strcat(command,"O|");
        }
        else if(sign==-1){ 
            return "INVL";   //game hasn't started yet.
        }
        
        command = realloc(command, (strlen(command)+strlen(row)+1));
        strcat(command,row);
        command = realloc(command, (strlen(command)+strlen(col)+1));
        strcat(command,col);

        free(row);
        free(col);
        free(secondField);

        return command;
    }

    else if(strcmp(command,"DRAW")==0){
        int nextIndex = retIndexOfNextField(buf,0);
        if(nextIndex==-1)
            return "INVL";
        if(nextIndex>=strlen(buf)){     //player is proposing a draw.
            free(command);
            char *temp = "DRAW|2|S|";
            command = malloc(strlen(temp)+1);
            strcpy(command,temp);
            return command;
        }
        else{                           //player is accepting or rejdecting a draw.
            char* decision = retNextField(buf,nextIndex);
            decision = realloc(decision, strlen(decision)+2);
            strcat(decision,"|");
            int length = strlen(decision);
            char* secondField = malloc(sizeof(char)*(findNumDigits(length)+2));
            sprintf(secondField,"%d|",length);

            command = realloc(command,strlen(command)+2);
            strcat(command,"|");
            command = realloc(command,(strlen(command)+strlen(secondField)+1));
            strcat(command,secondField);
            command = realloc(command,(strlen(command)+strlen(decision)+1));
            strcat(command,decision);

            free(secondField);
            free(decision);
            return command;
        }
        
    }

    else if(strcmp(command,"RSGN")==0){
        free(command);
        char *temp = "RSGN|0|";
        command = malloc(strlen(temp)+1);
        strcpy(command,temp);
        return command;
    }
    return "INVL";
}

static kind_t checkWinState(char *grid){
    char in[3][3];
        for(int i=0;i<9;++i){if( grid[i] == '.' ) grid[i] = ' ';}
        in[0][0] = grid[0];
        in[0][1] = grid[1];
        in[0][2] = grid[2];
        in[1][0] = grid[3];
        in[1][1] = grid[4];
        in[1][2] = grid[5];
        in[2][0] = grid[6];
        in[2][1] = grid[7];
        in[2][2] = grid[8];

     for(int i = 0;i<3;i++){
        if(!isspace(in[i][0]) && in[i][0]==in[i][1] && in[i][0]==in[i][2]){
            //if char is an 'x', thats the winner return X. Otherwise the winner is 'o' return O
            if(DEBUG)puts("Win by row.");
            return (in[i][0] == 'X' ? X : O);
        }
        if(!isspace(in[0][i]) && in[0][i]==in[1][i] && in[0][i]==in[2][i]){
            if(DEBUG)puts("Win by col.");
            return (in[0][i] == 'X' ? X : O);
        }
    }
    //now the diagonals->
    if(!isspace(in[0][0]) && in[0][0]==in[1][1] && in[0][0]==in[2][2]){
        if(DEBUG)puts("Win by diagnol.");
        return (in[1][1] == 'X' ? X : O);
        }
    
    if(!isspace(in[0][2]) && in[0][2]==in[1][1] && in[0][2]==in[2][0]){
        if(DEBUG)puts("Win by diagnol.");
        return (in[1][1] == 'X' ? X : O);
    }

    return no_win;
}

static kind_t checkDraw(char *grid)
{
    char in[3][3];
    for(int i=0;i<9;++i){if( grid[i] == '.' ) grid[i] = ' ';}
    in[0][0] = grid[0];
    in[0][1] = grid[1];
    in[0][2] = grid[2];
    in[1][0] = grid[3];
    in[1][1] = grid[4];
    in[1][2] = grid[5];
    in[2][0] = grid[6];
    in[2][1] = grid[7];
    in[2][2] = grid[8];

    for(int i = 0;i<3;i++){
        for(int j = 0;j<3;j++){
            if(isspace(in[i][j]))
                return no_draw;
        }
    }
    return draw;
}

int main(int argc, char **argv){
    int sockfd, bytes;
    char buf[BUFSIZE+1];
    if (argc != 3) {
        printf("Specify host and service\n");
        exit(EXIT_FAILURE);
    }

    sockfd = connect_inet(argv[1], argv[2]);
    if (sockfd < 0) exit(EXIT_FAILURE);
    if(DEBUG)printf("you are connected to the server.\n");

    install_handlers();
    pthread_mutex_init(&lock, NULL);
    handle_t *handle = malloc(sizeof(handle_t));
    init_msg(handle, sockfd);

    
    int status = 0;
    int flag = 0;
    //active -> is server still active, status-> INVL/valid message sent by user
    while(active && (bytes = read(STDIN_FILENO, buf, BUFSIZE)) > 0){


        if(buf[bytes-1] == '\n') buf[bytes-1] = '\0';
        else buf[bytes] = '\0';
        
        char* client_msg = malloc(strlen(buf)+1);
        strcpy(client_msg,buf);

        //user sent valid message to client, we can send it to server
        printf("client:%s\n",client_msg);
        if(strcmp(client_msg,"DRAW|2|S|")==0||strcmp(client_msg,"DRAW|2|R|")==0){
            status=1;
            flag=0;
        }
        write_serv_msg(sockfd,client_msg);

        wait:
            if(status)
            puts("waiting...");
            status=0;
        
        msg_data *server_msg = get_msg(handle);
        printf("server:%s\n",server_msg->msg);
        char msg_type[5]; strncpy(msg_type,server_msg->msg,4);
         msg_type[4] = '\0';

        if(strcmp(msg_type,"INVL")==0){
            printf("||Client wrote invalid message to server||\n");
            free_msg(server_msg);
            continue;
        }
        if(strcmp(msg_type,"WAIT")==0){
            printf("||Waiting for game to begin||\n");
            free_msg(server_msg);
            status=1;
            goto wait;
        }
        if(strcmp(msg_type,"BEGN")==0){
            char *opp_role;
            if(strcmp(server_msg->third_field,"X") == 0){
                sign = X;
                flag = 1;
                opp_role = "O";
            }
            else {
                sign = O;
                flag = 0;
                opp_role = "X";
            }
            printf("||Game has begun||\n[Your:%s & Your oponent %s is:%s]\nPlayer X goes first.\n",server_msg->third_field,server_msg->fourth_field,opp_role);
            free_msg(server_msg);
            if(!flag){
                status=1;
                goto wait;
            }
            continue;
        }
        if(strcmp(msg_type,"MOVD")==0){ 
            printf("||Player %s made a move (%s)||\n",server_msg->third_field,server_msg->fourth_field); 
            printf("Grid:|%s|\n",server_msg->fifth_field); 
            kind_t ret = checkWinState(server_msg->fifth_field);
            kind_t lev = checkDraw(server_msg->fifth_field);
            free_msg(server_msg); 
            if(ret != no_win){
                status=1;
                goto wait; 
            }
            else if(lev == draw){
                status=1;
                goto wait; 
            }
            if(flag){
                flag=0;
                status=1;
                goto wait; 
            }
            flag=1;
            continue; 
        }
        if(strcmp(msg_type,"DRAW")==0){
            printf("||Draw||\n");
            free_msg(server_msg);
             if(flag){
                flag=0;
                status=1;
                goto wait; 
            }
            flag=1;
            continue; 
        }
        if(strcmp(msg_type,"OVER")==0){
            printf("||GAME IS OVER||\n");
            free_msg(server_msg);
            break;
        }
    }
    free(handle);
    close(sockfd);
    return EXIT_SUCCESS;
}
