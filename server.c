/*Giorgos Dimos
 *15/05/2020
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define CHECK_MINUS_1(fun) if((fun) == -1){\
                            fprintf(stderr,"Server error %d at line %d ", errno, __LINE__);\
                            perror(":");\
                            return 1;\
                        }

#define CHECK_NULL(fun) if(!(fun)){\
                            fprintf(stderr,"Server error %d at line %d ", errno, __LINE__);\
                            perror(":");\
                            return 1;\
                        }
#define MAXREAD 512
#define MAXLINE 25

struct flightInfo{
    char airline[4];
    char airportDeparture[4];
    char airportArrival[4];
    int stops;
    int seats;
};

struct bookingInfo{
    pid_t pid;
    int reservedSeats;
};

static int getfileEntries(int fd);
static int filetoMem(int fd, const int *shm_p);
static int memtoFile(int fd, int *shm_p, int fileEntries);

/****************************************  Main  ***********************************************************************************/

int main(int argc, char *argv[]){

    key_t key, key_sem;
    int fd, fileEntries, shmid, *shm_p;
    int listening_socket;
    int maxAgents;
    struct sockaddr_un addr;
    pid_t pid;
    int n, dataSocket, flags, activeAgents;
    char buf[MAXREAD];

    if(argc!=3){
        printf("Invalid number of arguments.\n");
        printf("Please enter max number of agents and FlightInfo file\n");
        return 1;
    }

    maxAgents = atoi(argv[1]);
    struct pollfd fdinfo[maxAgents+2];  //array for enough agents, +1 for STDIN, +1 for listening socket
    
    struct bookingInfo bookings[maxAgents];

    CHECK_MINUS_1(fd=open(argv[2],O_RDONLY))

    fileEntries = getfileEntries(fd);

/**********************************  Create Shared Memory  *************************************************************************/

    CHECK_MINUS_1(key = ftok(".",'1'))
    CHECK_MINUS_1(shmid=shmget(key, sizeof(int)+(fileEntries-1)*sizeof(struct flightInfo),IPC_CREAT|IPC_EXCL|S_IRWXU))
    if((shm_p = (int *)shmat(shmid,NULL,0))==(void *) -1){
        fprintf(stderr,"errno: %d at line: %d\n", errno, __LINE__);
        return 1;
    }

/**********************************  Create Shemaphores  ***************************************************************************/
    int semid;
    CHECK_MINUS_1(key_sem = ftok(".",'2'))
    CHECK_MINUS_1((semid = semget(key_sem, 1, IPC_CREAT|IPC_EXCL|S_IRWXU)))
    CHECK_MINUS_1(semctl(semid,0,SETVAL,1)) // init to 0

/**********************************  Fill Shared Memory  ***************************************************************************/
    
    *shm_p = fileEntries;  //Enter array size to sharem memory
    if(filetoMem(fd, shm_p+1)){  //Enter array elements to shared memory
        fprintf(stderr,"errno: %d at line: %d\n", errno, __LINE__);
        return 1;
    }

    CHECK_MINUS_1(close(fd))

/**********************************  Creating Listening Socket  ********************************************************************/

    CHECK_MINUS_1(listening_socket=socket(AF_UNIX, SOCK_STREAM, 0))

    addr.sun_family=AF_UNIX;
    if(!getcwd(buf,255)){
        fprintf(stderr,"Error geting directory for socket\n");
        return 1;
    }
    strcat(buf,"/soc");
    strcpy(addr.sun_path, buf);
    CHECK_MINUS_1(bind(listening_socket, (struct sockaddr *)&addr, sizeof(addr)))
    CHECK_MINUS_1(listen(listening_socket,0))
    
/**********************************  Creating Data Sockets  ************************************************************************/

    int serverStop=0;  //Terminates server

    CHECK_NULL(memset(fdinfo, 0, sizeof(fdinfo)))

    CHECK_MINUS_1((flags = fcntl(listening_socket,F_GETFL)))
    CHECK_MINUS_1(fcntl(listening_socket,F_SETFL,flags|O_NONBLOCK))
    fdinfo[0].fd = STDIN_FILENO; fdinfo[0].events = POLLIN;
    fdinfo[1].fd = listening_socket; fdinfo[1].events = POLLIN;
    activeAgents = 2;
    printf("Server active\n");
    
    while (!serverStop) {

        //printf("calling poll\n");
        n = poll(fdinfo,activeAgents,5*1000);
        if (n < 0) { perror("poll"); return(1); }
        if (n == 0) { /*printf("timed out\n");*/ continue; }

        if (fdinfo[0].revents & (POLLIN|POLLHUP)) {  //read from input
            CHECK_MINUS_1((n=read(STDIN_FILENO, buf, MAXREAD)))
            if(n==0 || ((buf[0]=='Q' || buf[0]=='q') && buf[1]=='\n')){
                serverStop =1;
                continue;
            }
            else{
                printf("Invalid input from STDIN\n");
            }
        }

        if (fdinfo[1].revents & (POLLIN|POLLHUP)) {  //listening socket woke up
            printf("got new connection waiting\n");
            
            while((dataSocket=accept(listening_socket,NULL,NULL))>0){
                
                CHECK_MINUS_1(recv(dataSocket,&pid,sizeof(pid_t),0))
                if(activeAgents<maxAgents+2){  //accept new connection
                    bookings[activeAgents-2].pid = pid;
                    bookings[activeAgents-2].reservedSeats = 0;
                    fdinfo[activeAgents].fd = dataSocket;
                    fdinfo[activeAgents].events = POLLIN;
                    CHECK_MINUS_1(send(fdinfo[activeAgents].fd, &shmid, sizeof(int), 0))
                    CHECK_MINUS_1(send(fdinfo[activeAgents].fd, &semid, sizeof(int), 0))
                    activeAgents++;
                    printf("Accepted %d\n",pid);
                }
                else{  //deny new connection, server full
                    int deny = -1;
                    CHECK_MINUS_1(send(dataSocket, &deny, sizeof(int), 0))
                    CHECK_MINUS_1(close(dataSocket))
                    printf("Denied\n");
                }
            }
            if(errno!=11 && dataSocket==-1){
                fprintf(stderr,"errno: %d at line: %d\n", errno, __LINE__);
                return 1;
            }
            
        }

        for (int i=2; i< activeAgents; i++) {  //agent ready
            if (fdinfo[i].revents & (POLLIN|POLLHUP)) {
                printf("socket %d: ",i);
                int seats;
                CHECK_MINUS_1((n=recv(fdinfo[i].fd,&seats,sizeof(int),0)))
                if(n==0){  //check if agent terminated connection
                    printf("close socket\n");
                    CHECK_MINUS_1(close(fdinfo[i].fd))
                    fdinfo[i].fd = -1;
                    continue;
                }
                bookings[i-2].reservedSeats+=seats;
                printf("seats booked: %d\n", seats);
            }
        }

        for (int i=2; i< activeAgents; i++) {  //for terminated sockets
            if(fdinfo[i].fd==-1){
                for(int j=i; j<activeAgents-1; j++){  //rearrange the fd array for poll
                    fdinfo[j].fd = fdinfo[j+1].fd;
                    bookings[j-2].reservedSeats = bookings[j-1].reservedSeats;
                }
                activeAgents--;
                i--;
            }
        }
    }

/**********************************  Write Shared Memory to File  ******************************************************************/

    CHECK_MINUS_1(fd=open(argv[2],O_WRONLY))
    if(memtoFile(fd, shm_p+1, fileEntries)){
        fprintf(stderr,"errno: %d at line: %d\n", errno, __LINE__);
        return 1;
    }

    CHECK_MINUS_1(close(fd))

/**************************************  Print Total Bookings  *********************************************************************/

    for(int i=0;i<activeAgents-2; i++){
        printf("agent with pid %d booked %d seats in total\n",bookings[i].pid,bookings[i].reservedSeats);
    }

/***********************************  Clean up  ************************************************************************************/

    CHECK_MINUS_1(shmdt(shm_p))
    CHECK_MINUS_1(close(listening_socket))
    CHECK_MINUS_1(unlink("soc"))
    CHECK_MINUS_1(shmctl(shmid,IPC_RMID,NULL))
    CHECK_MINUS_1(semctl(semid,0,IPC_RMID))
    for (int i=2; i< activeAgents; i++) {
        CHECK_MINUS_1(close(fdinfo[i].fd))
    }

    return 0;
}

/**
 * @brief Counts the lines in the flight description file
 * 
 * @param file descriptor 
 * @return number of lines 
 */
static int getfileEntries(int fd){
    char buf[512], *p; 
    int fileEntries=0, readBytes;

    off_t offset = lseek(fd, 0 , SEEK_CUR);  //save original file descriptor offset
    if(offset==-1){
        fprintf(stderr,"errno: %d at line: %d\n", errno, __LINE__);
        return 1;
    }

    if(lseek(fd, 0 , SEEK_SET)==-1){ //set file descriptor offset to start
        fprintf(stderr,"errno: %d at line: %d\n", errno, __LINE__);
        return 1;
    }

    while((readBytes = read(fd,&buf,511))){
        if(readBytes==-1){
            fprintf(stderr,"errno: %d at line: %d\n", errno, __LINE__);
            return 1;
        }
        buf[readBytes] = '\0';
        p=buf;
        while((p = strchr(p,'\n'))){
            fileEntries++;
            p++;
        }
    }

    if(lseek(fd, offset , SEEK_SET)==-1){  //restore original file descriptor offset
        fprintf(stderr,"errno: %d at line: %d\n", errno, __LINE__);
        return 1;
    }

    return fileEntries;
}

/**
 * @brief Parses the flight description file and saves
 * the contents to memory
 * 
 * @param file descriptor
 * @param shared memory pointer
 * @return 0 for success, 1 for failure 
 */
static int filetoMem(int fd, const int *shm_p){
    int readBytes;
    struct flightInfo *runner;
    char buf[MAXREAD];

    runner = (struct flightInfo*) shm_p;
    char *c;
    while((readBytes = read(fd, buf, MAXREAD))){
        if(readBytes==-1){
            fprintf(stderr,"errno: %d at line: %d\n", errno, __LINE__);
            return 1;
        }

        if(buf[readBytes-1]!='\n'){  //convert buff to a string and 
            int i= readBytes-1;      //revert file descriptor to end of line
            while(i>0 && buf[i]!='\n') 
                i--;

            buf[i]='\0';
            CHECK_MINUS_1(lseek(fd, -(readBytes-i)+1, SEEK_CUR))
        }
        else{
            buf[readBytes-1] = '\0'; 
        }
        
        c = strtok(buf," ");
        do{
            strcpy(runner->airline, c);
            strcpy(runner->airportDeparture, strtok(NULL," "));
            strcpy(runner->airportArrival, strtok(NULL," "));
            runner->stops = atoi(strtok(NULL," "));
            runner->seats = atoi(strtok(NULL,"\n\0"));
            runner++;
        }while((c=strtok(NULL," ")));

    }
    return 0;
}

/**
 * @brief Saves the shared memory contents to a file
 * 
 * @param file descriptor 
 * @param shared memory pointer
 * @param number of file entries to be saved
 * @return 0 for success, 1 for failure  
 */
static int memtoFile(int fd, int *shm_p, int fileEntries){
    char str[MAXLINE], buf[MAXREAD]={'\0'};
    struct flightInfo *runner;

    CHECK_MINUS_1(lseek(fd,0,SEEK_SET))

    runner = (struct flightInfo *)shm_p;
    for(int i=0; i<fileEntries; i++){
        sprintf(str,"%s %s %s %d %d\n",runner[i].airline,runner[i].airportDeparture,runner[i].airportArrival, runner[i].stops, runner[i].seats);
        if(strlen(buf)+strlen(str)>MAXREAD){
            CHECK_MINUS_1(write(fd, buf, strlen(buf)))
            buf[0]= '\0';
        }
        strcat(buf,str);
    }
    CHECK_MINUS_1(write(fd, buf, strlen(buf)))

    return 0;
}