/*Giorgos Dimos
 *13/05/2020
 */

#include<stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>

struct flightInfo{
    char airline[4];
    char airportDeparture[4];
    char airportArrival[4];
    int stops;
    int seats;
};

#define MAXINPUT 25
#define CHECK_MINUS_1(fun) if((fun) == -1){\
                            fprintf(stderr,"Agent error %d at line %d ", errno, __LINE__);\
                            perror(":");\
                            return 1;\
                        }

static void find(char *src, char *dest, int num, struct flightInfo *array, int arraySize);
static void printFlightInfo(struct flightInfo f);
static int reserve(int socket, char *src, char *dest, char *airline, int num, struct flightInfo *array, int arraySize);

/**************************************************  Main  **************************************************************************************/

int main(int argc, char *argv[]){
    int s, arraySize, shmid, *p;
    struct sockaddr_un addr;
    pid_t pid;
    struct flightInfo *array;
    int semid, readBytes;
    struct sembuf op;
    char *input[5], buf[MAXINPUT];

    if(argc!=2){
        printf("Please enter server socket file as argument\n");
        printf("default is \"soc\"\n");
        return 1;
    }

/**********************************  Create Socket and initiate communication  ****************************************************************/

    CHECK_MINUS_1(s=socket(AF_UNIX, SOCK_STREAM, 0))

    addr.sun_family=AF_UNIX; 
    strcpy(addr.sun_path, argv[1]);
    CHECK_MINUS_1(connect(s, (struct sockaddr *)&addr, sizeof(addr)))

    pid = getpid();
    CHECK_MINUS_1(send(s, &pid, sizeof(pid_t), 0))

    CHECK_MINUS_1(recv(s, &shmid, sizeof(int), 0))

    if(shmid==-1){
        printf("Server denied connection\n");
        close(s);
        return 1;
    }
    printf("Connected\n");

/**********************************  Get shared memory and Semaphores info  ******************************************************************/

    p = (int *)shmat(shmid,NULL,0);
    if(!p){
        fprintf(stderr,"errno: %d at line: %d\n", errno, __LINE__);
        return 1;
    }
    arraySize = *p;
    array = (struct flightInfo *)(p+1);

    CHECK_MINUS_1(recv(s, &semid, sizeof(int), 0))

/****************************************  Read input from STDIN  *****************************************************************************/

    while((readBytes = read(STDIN_FILENO, buf, MAXINPUT-1))){
        buf[readBytes-1] = '\0'; //CHECK FOR VALID INPUT
        
        int i= 0;
        input[i] = strtok(buf," ");
        while(i<5 && input[i]!=NULL){  //break up words from input
            input[++i]= strtok(NULL, " ");
        }

        if(i==4 && !strcmp(input[0],"FIND")){
            printf("find\n");
            op.sem_num = 0; op.sem_op = -1; op.sem_flg = 0;
            CHECK_MINUS_1(semop(semid,&op,1))
            find(input[1], input[2], atoi(input[3]), array, arraySize);
            op.sem_num = 0; op.sem_op = 1; op.sem_flg = 0;
            CHECK_MINUS_1(semop(semid,&op,1))
        }
        else if(i==5 && !strcmp(input[0],"RESERVE")){
            printf("reserve\n");
            op.sem_num = 0; op.sem_op = -1; op.sem_flg = 0;
            CHECK_MINUS_1(semop(semid,&op,1))
            if(reserve(s, input[1], input[2], input[3], atoi(input[4]), array, arraySize)){
                printf("couldn't book seats\n");
            }
            op.sem_num = 0; op.sem_op = 1; op.sem_flg = 0;
            CHECK_MINUS_1(semop(semid,&op,1))
        }
        else if(i==1 && !strcmp(input[0],"EXIT")){
            break;
            printf("exit\n");
        }
        else{
            printf("invalid input\n");
        }
    }

/****************************************  Clean up  *****************************************************************************/

    CHECK_MINUS_1(close(s))
    CHECK_MINUS_1(shmdt(p))
    return 0;
}

static void find(char *src, char *dest, int num, struct flightInfo *array, int arraySize){

    for(int i=0;i<arraySize; i++){
        if(!strcmp(array[i].airportDeparture, src) ){
            if(!strcmp(array[i].airportArrival, dest) ){
                if(array[i].seats>=num){
                    printFlightInfo(array[i]);
                }
            }
        }
    }
}

static int reserve(int socket, char *src, char *dest, char *airline, int num, struct flightInfo *array, int arraySize){
    for(int i=0;i<arraySize; i++){
        if(!strcmp(array[i].airportDeparture, src) ){
            if(!strcmp(array[i].airportArrival, dest) ){
                if(!strcmp(array[i].airline, airline) ){
                    if(array[i].seats>=num){
                        printf("found flight\n");
                        array[i].seats-=num;
                        CHECK_MINUS_1(send(socket, &num, sizeof(int), 0))
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

static void printFlightInfo(struct flightInfo f){
        printf("%s ",f.airline);
        printf("%s ",f.airportDeparture);
        printf("%s ",f.airportArrival);
        printf("%d ",f.stops);
        printf("%d\n",f.seats);
}