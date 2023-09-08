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

#define DEBUG 0 // Debugging purposes
#define MSG 0 // Debugging purposes

int player_capacity; 
int player_count;



int retIndexOfNextField(char* in, int current)
{
    int i = current;
    while(i<strlen(in)&&in[i]!='|'){
        i++;
    }
    if(i==(strlen(in)-1) && in[i] != '|'){
        return -1;
    }
    return i+1;
}

char* retNextField(char* in, int index)
{
    int i = index;
    while(i<strlen(in)&&in[i]!='|')
    {
        i++;
    }
    char* out = malloc(sizeof(char)*(i-index+1));
    int j = index;
    int a = 0;
    while(a<(i-index)){
        out[a]=in[j];
        a++;
        j++;
    }
    out[i-index]='\0';
    return out;
}

void init_msg(handle_t *handle, int fd)
{
    handle->fd = fd;
    memset(handle->buffer,0,BUFSIZE);
    handle->buffer[0] = '\0';
    handle->real_len = 0;
    handle->b_count = 0;
    handle->start = 0;
}

void write_serv_msg(int sockfd, char * msg)
{
    int n = write(sockfd, msg, strlen(msg));
    if (n < 0) perror("ERROR writing msg to client socket");
    free(msg); 
}

void write_cli_msg(int sockfd, char * send, msg_data *recv)
{
    int n = write(sockfd, send, strlen(send));
    if (n < 0) perror("ERROR writing msg to client socket");
    free(send);
    free_msg(recv);
}

int check_field1(char *buf, int v_count, msg_t type)
{   
    int j = 0;
    int f_count = 2;    
    //we have already check outside of this function for WAIT & RSGN (special case)
    if(type==WAIT||type==RSGN) return 1; 

    for(int i = v_count; i < strlen(buf); ++i){
        if(buf[i]=='|') ++f_count;
        if(MSG)printf("[%c: field_count%d]\n",buf[i],f_count);
        switch(type){
            case RSGN:
            //shouldn't get this far
                return 0; 
            case WAIT:
            //shouldn't get this far
                return 0;
            case PLAY:
                if(f_count > 3) return 0;
                else if(buf[i] == '|') return 1;
                //don't really need to check the players name
                break;
            case MOVE:
                if(f_count > 4) return 0;
                else if(buf[i] == '|' && f_count == 4){return 1;}            
                break;
            case DRAW:
                if(f_count > 3) return 0;
                else if(buf[i] == '|'){return 1;}
                break;
            case BEGN:
                if(f_count > 4) return 0;                   
                else if(buf[i] == '|' && f_count == 4){return 1;} 
                //don't really need to check the players name (can check in game)
                break;
            case MOVD:
                if(f_count > 5) return 0;
                else if(buf[i] == '|' && f_count == 5){return 1;}  
                /*FIX ME, need to check grid*/
                break;
            case INVL:
                if(f_count > 3) return 0;
                if(buf[i] == '|') return 1;
                //don't really need to reason message
                break;
            case OVER:
                if(f_count > 4) return 0;
                else if(buf[i] == '|' && f_count == 4){return 1;}                      
                //don't really need to reason message 
                break;
        }
        ++j;
    }
    return 2;
}

int check_field2(char *buf, int v_count, msg_t type)
{   
    int j = 0;
    int f_count = 2;    
    //we have already check outside of this function for WAIT & RSGN (special case)
    if(type==WAIT||type==RSGN) return 1; 

    for(int i = v_count; i < strlen(buf); ++i){
        if(buf[i]=='|') ++f_count;
        if(MSG)printf("[%c: field_count%d]\n",buf[i],f_count);
        switch(type){
            case RSGN:
            //shouldn't get this far
                return 0; 
            case WAIT:
            //shouldn't get this far
                return 0;
            case PLAY:
                if(f_count > 3) return 0;
                else if(buf[i] == '|') return 1;
                //don't really need to check the players name
                break;
            case MOVE:
                if(f_count > 4) return 0;
                else if(j == 0 && buf[i] != 'X' && buf[i] != 'O' ) return 0; // Check role
                else if(j == 1 && buf[i] != '|' ) return 0; // Check vertical bar 
                else if(j == 2 && buf[i] != '1' && buf[i] != '2' && buf[i] != '3') return 0; // digit
                else if(j == 3 && buf[i] != ',') return 0; // comma 
                else if(j == 4 && buf[i] != '1' && buf[i] != '2' && buf[i] != '3') return 0; // digit
                else if(j == 5 && buf[i] != '|') return 0; // vertical bar
                else if(buf[i] == '|' && f_count == 4){return 1;}            
                break;
            case DRAW:
                if(f_count > 3) return 0;
                else if(j == 0 && buf[i] != 'S' && buf[i] != 'R' && buf[i] != 'A') return 0;
                else if(buf[i] == '|'){return 1;}
                break;
            case BEGN:
                if(f_count > 4) return 0;                   
                else if(j == 0 && buf[i] != 'X' && buf[i] != 'O' ) return 0; // Check role
                else if(j == 1 && buf[i] != '|' ) return 0; // Check vertical bar 
                else if(buf[i] == '|' && f_count == 4){return 1;} 
                //don't really need to check the players name (can check in game)
                break;
            case MOVD:
                if(f_count > 5) return 0;
                else if(j == 0 && buf[i] != 'X' && buf[i] != 'O' ) return 0; // Check role
                else if(j == 1 && buf[i] != '|' ) return 0; // Check vertical bar 
                else if(j == 2 && buf[i] != '1' && buf[i] != '2' && buf[i] != '3') return 0; // digit
                else if(j == 3 && buf[i] != ',') return 0; // comma 
                else if(j == 4 && buf[i] != '1' && buf[i] != '2' && buf[i] != '3') return 0; // digit
                else if(j == 5 && buf[i] != '|') return 0; // vertical bar
                else if(j >= 6 && j <= 14 && buf[i] != 'X' && buf[i] != 'O' && buf[i] != '.') return 0; //grid cells
                else if(buf[i] == '|' && f_count == 5){return 1;}  
                /*FIX ME, need to check grid*/
                break;
            case INVL:
                if(f_count > 3) return 0;
                if(buf[i] == '|') return 1;
                //don't really need to reason message
                break;
            case OVER:
                if(f_count > 4) return 0;
                if(j == 0 && buf[i] != 'W' && buf[i] != 'L' && buf[i] != 'D' ) return 0; // Check role
                else if(j == 1 && buf[i] != '|' ) return 0; // Check vertical bar
                else if(buf[i] == '|' && f_count == 4){return 1;}                      
                //don't really need to reason message 
                break;
        }
        ++j;
    }
    return 2;
}

int get_len(char *buf, int v_count, msg_t type)
{
    int j = 0;
    int f_count = 2;

    for(int i = v_count; i < strlen(buf); ++i){
        if(buf[i]=='|') ++f_count;
        switch(type){
            case RSGN:
            //shouldn't get this far
                return 0; 
            case WAIT:
            //shouldn't get this far
                return 0;
            case PLAY:
                if(f_count > 3) return 0;
                else if(buf[i] == '|') return ++j;
                //don't really need to check the players name
                break;
            case MOVE:
                if(f_count > 4) return 0;
                else if(buf[i] == '|' && f_count == 4){return ++j;}            
                break;
            case DRAW:
                if(f_count > 3) return 0;
                else if(buf[i] == '|'){return ++j;}
                break;
            case BEGN:
                if(f_count > 4) return 0;                   
                else if(buf[i] == '|' && f_count == 4){return ++j;} 
                //don't really need to check the players name (can check in game)
                break;
            case MOVD:
                if(f_count > 5) return 0;
                else if(buf[i] == '|' && f_count == 5){return ++j;}  
                break;
            case INVL:
                if(f_count > 3) return 0;
                if(buf[i] == '|') return ++j;
                break;
            case OVER:
                if(f_count > 4) return 0;
                else if(buf[i] == '|' && f_count == 4){return ++j;}                      
                break;
        }
        ++j;
    }
    return 0;
}

//this will become new (get_msg) function when its done
msg_data * get_msg(handle_t *handle)
{   
    char *buf = handle->buffer;
    int sockfd = handle->fd;

    if(MSG)puts("-- Start msg Function --");
    char msg_type[5], int_str[4];
    //handle->b_count = total bytes read, v_count = length of 1/2 field
    int stated_len, v_count = 0, bytes = 0, status = 0;
    msg_t type;
    msg_data *msg = NULL;
    
    if(handle->real_len == 0){init_msg(handle, handle->fd);}
    while(1){
        v_count = 0;
        for(int i = handle->start; i < strlen(buf); ++i){
            if(MSG)printf("[%c]\n",buf[i]);
                 if(i == handle->start + 0 && buf[i] != 'P' && buf[i] != 'M' && buf[i] != 'R' && buf[i] != 'D' && buf[i] != 'W' && buf[i] != 'B' && buf[i] != 'M' && buf[i] != 'I' && buf[i] != 'O'){if(MSG)puts("Field 1 error.");init_msg(handle, handle->fd);  return NULL;} // first char
            else if(i == handle->start + 1 && buf[i] != 'L' && buf[i] != 'O' && buf[i] != 'S' && buf[i] != 'R' && buf[i] != 'A' && buf[i] != 'E' && buf[i] != 'O' && buf[i] != 'N' && buf[i] != 'V'){if(MSG)puts("Field 1 error.");init_msg(handle, handle->fd);  return NULL;} // second char
            else if(i == handle->start + 2 && buf[i] != 'A' && buf[i] != 'V' && buf[i] != 'G' && buf[i] != 'A' && buf[i] != 'I' && buf[i] != 'G' && buf[i] != 'V' && buf[i] != 'V' && buf[i] != 'E'){if(MSG)puts("Field 1 error.");init_msg(handle, handle->fd);  return NULL;} // third char
            else if(i == handle->start + 3 && buf[i] != 'Y' && buf[i] != 'E' && buf[i] != 'N' && buf[i] != 'W' && buf[i] != 'T' && buf[i] != 'N' && buf[i] != 'D' && buf[i] != 'L' && buf[i] != 'R'){if(MSG)puts("Field 1 error.");init_msg(handle, handle->fd);  return NULL;} // fourth
            else if(i == handle->start + 4 && buf[i] != '|'){if(MSG)puts("Field 1 error.");init_msg(handle, handle->fd);  return NULL;}
            else if(i == handle->start + 5){ // first digit

            if(isdigit(buf[i]) > 0) int_str[0] = buf[i];
            else {if(MSG)puts("Field 2 error.");init_msg(handle, handle->fd);  return NULL;}
            } 
            else if(i == handle->start + 6){ // second digit or '|'
                if(isdigit(buf[i]) > 0) int_str[1] = buf[i];
                else if(buf[i] == '|') {int_str[1] = '\0'; status = 1; ++v_count; break;}
                else{if(MSG)puts("Field 2 error.");init_msg(handle, handle->fd);  return NULL;}
            }
            else if(i == handle->start + 7){ // third digit or '|'
                if(isdigit(buf[i]) > 0) int_str[2] = buf[i];
                else if(buf[i] == '|') {int_str[2] = '\0'; status = 1; ++v_count; break;}
                else {if(MSG)puts("Field 2 error.");init_msg(handle, handle->fd);  return NULL;}
            }
            else if(i == handle->start + 8){ // fourth digit or '|'
                if(buf[i] == '|') {int_str[3] = '\0'; status = 1; ++v_count;break;}
                else {if(MSG)puts("Field 2 error.");init_msg(handle, handle->fd);  return NULL;}
            }
            ++v_count;
            //if length of buf is filled up we reset position where read will write to in the buffer 
        }
        
        if(status)break;
        
        if(BUFSIZE - handle->b_count == 0){if(DEBUG)printf("Buffer full.\n");  init_msg(handle, handle->fd);continue;}
        char *ss = buf + handle->b_count;
        int dd = BUFSIZE - handle->b_count;
        
        bytes = read(sockfd, ss, dd);
        if (bytes == 0){ {printf("got EOF\n");}  init_msg(handle, handle->fd); return NULL;} 
        else if (bytes == -1) { {printf("Terminating: %s\n", strerror(errno));} init_msg(handle, handle->fd); return NULL;}
        
        handle->b_count += bytes;
        buf[handle->b_count] = '\0';
        if(MSG)printf("[%s]\nBytes read:%d  Position:%d  Length remaining:%d  buf[%d]:%c\n",buf,bytes,handle->b_count,(BUFSIZE - handle->b_count),(handle->b_count-1),buf[handle->b_count-1]);
        

    }
    

    stated_len = atoi(int_str); //get the stated length from the message in an integer
    if(!(stated_len >= 0 && stated_len <= 255) ){ if(MSG)printf("0-255 only.\n"); init_msg(handle, handle->fd); return NULL;}
    handle->real_len = handle->b_count - v_count - handle->start; //actual remaining length that can be read from buffer
    memmove(msg_type,buf+handle->start,4); msg_type[4] = '\0'; //get our msg_type in a string
    if(MSG)printf("Type:[%s] 1st & 2nd byte total:%d {stated:%d Remaining in the buffer:%d}\n",msg_type,v_count, stated_len, handle->real_len);
    if(strcmp(msg_type,"PLAY")==0) type = PLAY; else if(strcmp(msg_type,"MOVE")==0) type = MOVE; else if(strcmp(msg_type,"RSGN")==0) type = RSGN; else if(strcmp(msg_type,"DRAW")==0) type = DRAW; else if(strcmp(msg_type,"WAIT")==0) type = WAIT; else if(strcmp(msg_type,"BEGN")==0) type = BEGN; else if(strcmp(msg_type,"MOVD")==0) type = MOVD; else if(strcmp(msg_type,"INVL")==0) type = INVL; else if(strcmp(msg_type,"OVER")==0) type = OVER;
    status = 0;
    if( stated_len != 0 && (type==RSGN || type==WAIT) ){if(MSG)puts("FC:error");init_msg(handle, handle->fd);  return NULL;} //Check if RSGN or WAIT have a stated length thats not zero
    else if( stated_len == 0 && !(type==RSGN || type==WAIT) ){if(MSG)puts("FC:error");init_msg(handle, handle->fd);  return NULL;} //Check if other msg_type have a stated length thats zero
    while(handle->real_len < stated_len){ //real remaining length of the message is less than the stated length (we need to read again, we field check so we can't discard message just yet)
        //Need to do a field check everytime before we read
        status = check_field1(handle->buffer,handle->start+v_count, type);
        if(MSG)printf("[1:stat %d]\n",status);
        if( status == 1 ) break;
        else if(status == 0){if(MSG)puts("FC:error");init_msg(handle, handle->fd);  return NULL;}

        if(BUFSIZE - handle->b_count == 0){if(DEBUG)printf("Buffer full.\n"); init_msg(handle, handle->fd);continue;}
        bytes = read(sockfd, buf + handle->b_count, BUFSIZE - handle->b_count);
        if (bytes == 0){ {printf("got EOF\n");}  init_msg(handle, handle->fd);  return NULL;} 
        else if (bytes == -1) { {printf("Terminating: %s\n", strerror(errno));}  init_msg(handle, handle->fd);  return NULL;}
        handle->b_count += bytes;
        handle->real_len += bytes;
        buf[handle->b_count] = '\0';

        if(MSG)printf("[%s]\nBytes read:%d  Position:%d  Length remaining:%d  buf[%d]:%c\n",buf,bytes,handle->b_count,(BUFSIZE - handle->b_count),(handle->b_count-1),buf[handle->b_count-1]);
    }

        if(MSG)printf("[%s] Bytes read:%d  Position:%d  Length remaining:%d {stated:%d real:%d}\n",buf,bytes,handle->b_count,(BUFSIZE - handle->b_count),stated_len, handle->real_len);

    int match_len = get_len(handle->buffer, handle->start+v_count, type);
    if(MSG)printf("Real length of MSG:%d ... stated:%d\n",match_len,stated_len);
    if(match_len!=stated_len){if(MSG)puts("FC:Stated length does not match");init_msg(handle, handle->fd);  return NULL;}

    status = check_field2(handle->buffer, handle->start+v_count, type);
    if(MSG)printf("[2:stat %d]\n",status);
    

    //get the length of the whole message
    int msg_Length=0, f_count=0, index=0;
    for(int i=0;i<stated_len;++i) msg_Length = v_count+i;
    ++msg_Length;
    if(stated_len == 0) msg_Length = v_count;
    handle->real_len = handle->real_len - match_len; //actual remaining length that can be read from buffer
    if(MSG)printf("bytes in buffer:%d left to read:%d length of the msg:%d 1/2 field bytes:%d\n",handle->b_count,handle->real_len, msg_Length, v_count);
    char tempMessage[BUFSIZE+1];
    memmove(tempMessage,buf+handle->start,msg_Length); tempMessage[msg_Length] = '\0';

    //the handle->start of the new message
    handle->start += msg_Length;
    if(MSG)printf("Start of new msg:[%c]\n",buf[handle->start]);
    
    
    //ensure field count on message
    for(int i =0;index<strlen(tempMessage);++i){++f_count; char* field = retNextField(tempMessage,index); index = retIndexOfNextField(tempMessage,index); free(field);}
    index = 0;
    if((strcmp(msg_type,"PLAY")==0 && f_count !=3) || (strcmp(msg_type,"MOVE")==0 && f_count !=4) || (strcmp(msg_type,"RSGN")==0 && f_count !=2) || (strcmp(msg_type,"DRAW")==0 && f_count !=3) || (strcmp(msg_type,"WAIT")==0 && f_count !=2) || (strcmp(msg_type,"BEGN")==0 && f_count !=4) || (strcmp(msg_type,"MOVD")==0 && f_count !=5) || (strcmp(msg_type,"INVL")==0 && f_count !=3) || (strcmp(msg_type,"OVER")==0 && f_count !=4)){if(MSG)puts("FC:malformed");init_msg(handle, handle->fd);  return NULL;}
    
    if(status == 0){if(MSG)puts("FC:invalid content");
        char * temp = "dump";
        char * invalid = malloc(strlen(temp)+1);
        strcpy(invalid, temp);
        msg = malloc(sizeof(msg_data)*1);
        msg->msg = invalid;
        msg->type = RSGN;
        return msg;
    }

    msg = malloc(sizeof(msg_data)*1);
    char *whole_msg = malloc(strlen(tempMessage)*sizeof(char)+1);
    strcpy(whole_msg,tempMessage);
    msg->type = type; 
    msg->msg = whole_msg;
    index = 0;
    for(int i =0;index<strlen(tempMessage);++i) {
    char* field = retNextField(tempMessage,index);
    index = retIndexOfNextField(tempMessage,index);
    if(MSG)printf("%s %d\n",field,i);
    if(i == 2){msg->third_field = field;continue;}
    if(i == 3){msg->fourth_field = field;continue;}
    if(i == 4){msg->fifth_field = field;continue;}
    free(field);
    }    
    if(DEBUG)printf("Protocol Message:(%s)\n",msg->msg);  
    if(MSG)printf("-- Leaving msg Function --\n");
    return msg;
}

void free_msg(msg_data *data)
{
    if(data == NULL) return;
    free(data->msg);
    switch((data->type)){
        case RSGN:
            break;
        case WAIT:
            break;
        case PLAY:
            free((data->third_field));
            break;
        case DRAW:
            free((data->third_field));
            break;
        case INVL:
            free((data->third_field));
            break;
        case MOVE:
            free((data->third_field));
            free((data->fourth_field));
            break;
        case OVER:
            free((data->third_field));
            free((data->fourth_field));
            break;
        case BEGN:
            free((data->third_field));
            free((data->fourth_field));
            break;
        case MOVD:
            free((data->third_field));
            free((data->fourth_field));
            free((data->fifth_field));
            break;
    }
   free(data);
}


player_t *init_player(unsigned capacity)
{
    player_capacity = capacity;
    player_count = 0;
    player_t *list = malloc( sizeof(player_t) * capacity );
    for(int i = 0; i < capacity; ++i ){
        list[i].avail = FREE;
    }
    return list;
}

int get_count(){
    return player_count;
}

void dest_player(player_t *list)
{
    for(int i = 0; i < player_capacity; ++i ){
        if(list[i].avail == IN_USE){
            free(list[i].name);
        }
    }
    free(list);
}

int create_player(player_t *list,char *name)
{
    int index, check = 1;
    if(DEBUG)printf("Player count:%d -- Capacity:%d\n",player_count,player_capacity);
    if(player_count == 0){
        if(DEBUG)puts("No players yet");
        char *player_name = malloc(strlen(name)+1);
        strcpy(player_name,name);
        ++player_count;
        list[player_count-1].avail = IN_USE;
        list[player_count-1].name = player_name;
        index = player_count-1;
    }    
    else if(player_count>0){ 
        //check if name is being used 
        if(check){
            for(int i=0; i<player_capacity; ++i){
                if(list[i].avail == IN_USE){
                    //check if name is being used
                    if(strcmp(name,list[i].name)==0){
                        index = -1;
                        check = 0; 
                        if(DEBUG)puts("name in use.");
                        break;
                    }
                }
            }
        }
        //name not in use, look for a free spot place it
        if(check){
            for(int i=0; i<player_capacity; ++i){
                if(list[i].avail == FREE){
                    ++player_count;
                    char *player_name = malloc(strlen(name)+1);
                    strcpy(player_name,name);
                    list[i].avail = IN_USE;
                    list[i].name = player_name;
                    index = i;
                    check = 0; 
                    if(DEBUG)puts("Free spot.");
                    break;
                }
            }
        }
        if(check){
            //no free spot
            if(DEBUG)puts("no free spot.");
            index = -2;
        }
    }
    if(DEBUG)printf("Player count:%d -- Capcity:%d\n",player_count,player_capacity);
    return index; 
}

int remove_player(player_t *list)
{
    if(DEBUG)printf("Player count:%d -- Capcity:%d\n",player_count,player_capacity);
    if(player_count == 0) return -1;
    (*list).avail = FREE; 
    free((*list).name);
    --player_count;
    if(DEBUG)printf("Player count:%d -- Capcity:%d\n",player_count,player_capacity);
    return 1;
}

void print_p( player_t *list)
{
    for(int i =0; i < player_capacity; ++i){
        printf("%d:%d",i,list[i].avail);
        if(list[i].avail==IN_USE) printf("%s\n",list[i].name);
        else printf("\n");
    }
}
