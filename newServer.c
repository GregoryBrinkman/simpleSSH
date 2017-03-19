//test macros
#define _POSIX_C_SOURCE 199309L
#define _XOPEN_SOURCE 600
#define _GNU_SOURCE



//header files
#include <sys/syscall.h>
#include <sys/epoll.h>
#include <errno.h>
#include <pthread.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>

//constants
#define ENTER_NAME "Server: Enter Your Name.\n"
#define EXIT "exit"
#define TALK "talk"
#define CONF_TALK "conference talk"
#define NOT_LOGGED_IN " is not logged in.\n"
#define BUFFSIZE 4096
#define MAX_NUM_CLIENTS 3

#define DEBUG

//Hold Client Data here!
struct client
{
  char* ip;
  char* name;
  int   inCall;
};

struct client clients[MAX_NUM_CLIENTS];


//global variables
int numClients = 0;

//function declarations
int save_client(int, char *);
void *handle_client(void*);
void accepted_client(int, int);
void conf_talk(int, int, char*);
void talk(int, int, char*);

int main(int argc, char *argv[])
{

  //
  //test number of arguments
  //
  int port;
  if(argc != 2){
    fprintf(stderr, "USAGE: program portnumber\n");
    exit(EXIT_FAILURE);
  }else{
    port = atoi(argv[1]);
  }

  //
  //variable instantiation
  //
  int server_sockfd, client_sockfd;
  int result;
  socklen_t server_len, client_len;
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;
  pthread_attr_t attr;

  //
  //set thread to start in detached mode
  //
  if(pthread_attr_init(&attr) != 0){
    perror("pthread attr");
    exit(EXIT_FAILURE);
  }

  if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0){
    perror("pthread attr setdetachstate");
    exit(EXIT_FAILURE);
  }

  //
  //socket setup
  //
  if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    perror("Socket Failed");
    exit(EXIT_FAILURE);
  }
  //
  //Reuse socket when server is closed (for testing)
  //
  int i = 1;
  setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

  //
  //set variables for socket bind to port
  //
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(port);
  server_len = sizeof(server_address);

  if((result = bind(server_sockfd, (struct sockaddr *)&server_address,
          server_len)) == -1){
    perror("Bind refused");
    exit(EXIT_FAILURE);
  }

  //
  //creation of listening socket
  //
  if((result = listen(server_sockfd, 5)) != 0){
    perror("Listen Failed");
    exit(EXIT_FAILURE);
  }

  signal(SIGCHLD, SIG_IGN);
  client_len = sizeof(client_address);

  //
  //accept loop
  //
  printf("Server Listening On port %i\n", port);
  while(1) {
    client_sockfd = accept4(server_sockfd, (struct sockaddr *)&client_address, &client_len, SOCK_CLOEXEC);

    char *ip = inet_ntoa(client_address.sin_addr);
    if(client_sockfd != -1)
    {
      printf("Accepted Request from ip: %s\n", ip);

      pthread_t handle_thread;

      //
      // pass in file descriptor to thread
      //
      int *fd = malloc(sizeof(int));
      *fd = client_sockfd;

      //
      //create thread running handle_client in detached mode
      //
      pthread_create(&handle_thread, &attr, handle_client, fd);

#ifdef DEBUG
      printf("pthread created: %ld, connect_fd is %i\n", (long) handle_thread, client_sockfd);
#endif

    }
  }
}


void *handle_client(void * arg){

  int connect_fd = *(int*)arg;
  free(arg);


#ifdef DEBUG
  printf("connect_fd = %i\n", connect_fd);
#endif


  char buff[BUFFSIZE];
  //
  //handshake
  //
  printf("Message Sent Asking for Client-Name\n");
  write(connect_fd, ENTER_NAME, strlen(ENTER_NAME));

  if(0 == read(connect_fd, &buff, BUFFSIZE)){
    close(connect_fd);
  }
  //
  // parsing newlines and carriage returns
  //
  for(int i = 0; i < BUFFSIZE; i++){
    if(buff[i] == '\n' || buff[i] == '\r'){
      buff[i] = '\0';
      break;
    }
  }


  //
  // save name and ip address of client
  // return the index of the newly saved client in 'clients' array
  //
  int clients_num = save_client(connect_fd, buff);
  accepted_client(clients_num, connect_fd);
  pthread_exit(NULL);
}

int save_client(int fd, char* buff)
{
  socklen_t len;
  struct sockaddr_in addr;

  //
  // Get client ip from client file descriptor
  //
  getsockname(fd,(struct sockaddr *)&addr, &len);
  char *ip = inet_ntoa(addr.sin_addr);


  //
  // save client
  //
  clients[numClients].ip = ip;
  clients[numClients].name = buff;
  clients[numClients].inCall = 0;
  numClients++;

  printf("%s named %s\n", clients[numClients-1].ip, clients[numClients-1].name);
  return numClients-1;
}

void accepted_client(int clients_num, int connect_fd)
{
  char buff[BUFFSIZE];

  //
  // Welcome message. Trust me, it's easier this way.
  //
  write(connect_fd, "Hi, ", 4);
  write(connect_fd, clients[numClients-1].name, strlen(clients[numClients-1].name));
  write(connect_fd, "!\n", 2);


  //
  // read client input forever (until 'exit')
  //
  while(1){
    read(connect_fd, &buff, BUFFSIZE);

    //
    // parsing newlines and carriage returns
    //
    for(int i = 0; i < BUFFSIZE; i++){
      if(buff[i] == '\n' || buff[i] == '\r'){
        buff[i] = '\0';
        break;
      }
    }

    //
    //'talk client'
    //
    if(0 == strncmp(buff, TALK, strlen(TALK)))
      talk(clients_num, connect_fd, buff);
    //
    //'conference talk client1 client2'
    //
    if(0 == strncmp(buff, CONF_TALK, strlen(CONF_TALK)))
      conf_talk(clients_num, connect_fd, buff);
    //
    // 'exit'
    //
    if(0 == strncmp(buff, EXIT, strlen(EXIT)))
      break;
  }

  //Ya gotta close those files.
  close(connect_fd);
}

void talk(int clients_num, int fd, char* oldBuff){

  //test data
  clients[numClients].ip = "111.222.333.444";
  clients[numClients].name = "client";
  clients[numClients].inCall = 0;
  numClients++;
  //test data

  char buff[BUFFSIZE];
  //
  // cut 'talk ', save client name to buff
  //
  int flag = 0, i = 0, j = strlen(TALK) + 1;

  //Hey! Look at that! (This copies the old buffer into the new buffer, Null terminates the loop.)
  while((buff[i++] = oldBuff[j++])){}


  //
  //check clients array for desired client
  //
  for(i = 0; i < numClients; i++){
    if(i == clients_num){continue;}
    int ret = strcmp(buff, clients[i].name);
    printf("%i\n", ret);

    //client found
    if(ret == 0){
      flag++;
      write(fd, "yo\n", 3);
    }
  }

  //Was a client found?
  //Yes! Client found!
  if(flag){

  //No clients here! Return to accepted_client
  }else{
    write(fd, buff, strlen(buff));
    write(fd, NOT_LOGGED_IN, strlen(NOT_LOGGED_IN));
  }

}
void conf_talk(int clients_num, int fd, char* oldBuff){
  char buff[BUFFSIZE];
  int i = 0;
  int j = strlen(CONF_TALK) + 1;
  while((buff[i++] = oldBuff[j++])){}
  write(fd, buff, strlen(buff));
}
