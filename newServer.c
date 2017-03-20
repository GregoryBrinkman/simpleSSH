//test macros
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
#define ESTABLISHED "Talk connection established with "
#define CONF_TERMINATED "Conference terminated by "
#define TERMINATED "Connection terminated with "
#define CONF_TALK "conference talk"
#define NOT_LOGGED_IN " is not logged in.\n"
#define BUFFSIZE 4096
#define MAX_NUM_CLIENTS 3

/* #define DEBUG */

//Hold Client Data here!
struct client
{
  char* ip;
  char* name;
  int   fd;
  int   client1;
  int   client2;
  int   inCall;
  int   ringing;
  int   index;
};

struct client clients[MAX_NUM_CLIENTS];


//global variables
int numClients = 0;

//function declarations
int save_client(int, char *);
void *handle_client(void*);
void accepted_client(int);
void conf_talk(int, char*);
void talk(int, char*);
int check_for_client(int, char*);
void process_talk(int, int, char*);
void establish_connection(int, int);
void accept_call(int, char*);
void converse(int);
void establish_conf_connection(int, int, int);
void process_conf_talk(int, int, char*, int, char*);

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
  accepted_client(clients_num);
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
  clients[numClients].fd = fd;
  clients[numClients].client1 = -1;
  clients[numClients].client2 = -1;
  clients[numClients].name = buff;
  clients[numClients].inCall = 0;
  clients[numClients].ringing = 0;
  clients[numClients].index = numClients;
  numClients++;

  printf("%s named %s\n", clients[numClients-1].ip, clients[numClients-1].name);
  return numClients-1;
}

void accepted_client(int clients_num)
{

  //
  // Welcome message. Trust me, it's easier this way.
  //
  write(clients[clients_num].fd, "Hi, ", 4);
  write(clients[clients_num].fd, clients[numClients-1].name, strlen(clients[numClients-1].name));
  write(clients[clients_num].fd, "!\n", 2);


  //
  // read client input forever (until 'exit')
  //
  while(1){
    char* buff = malloc(BUFFSIZE);
    read(clients[clients_num].fd, buff, BUFFSIZE);

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
    // accept incoming call
    //
    if(clients[clients_num].ringing){
      accept_call(clients_num, buff);
      free(buff);
      continue;
    }
    //
    //'talk client'
    //
    if(0 == strncmp(buff, TALK, strlen(TALK))){
      talk(clients_num, buff);
      free(buff);
    }
    //
    //'conference talk client1, client2'
    //
    if(0 == strncmp(buff, CONF_TALK, strlen(CONF_TALK))){
      conf_talk(clients_num, buff);
      free(buff);
    }
    //
    // 'exit'
    //
    if(0 == strncmp(buff, EXIT, strlen(EXIT))){
      free(buff);
      break;
    }
  }

  //Ya gotta close those files.
  close(clients[clients_num].fd);
  clients[clients_num].fd = -1;
  clients[clients_num].name = "sa;ldfkjads;lkfjas;ldkfjsa";
  clients[clients_num].ip = "sa;ldfkjads;lkfjas;ldkfjsa";
}





void talk(int clients_num, char* oldBuff){

  printf("Talk request recieved from %s\n", clients[clients_num].name);

  char* buff = malloc(BUFFSIZE);
  //
  // cut 'talk ', save client name to buff
  //
  int client_index = 0, i = 0, j = strlen(TALK) + 1;

  //Hey! Look at that! (This copies the old buffer into the new buffer, Null terminates the loop.)
  while((buff[i++] = oldBuff[j++])){}

  //
  // returns -1 if no client is found
  // returns clients index if client is found
  //
  client_index = check_for_client(clients_num, buff);
  process_talk(clients_num, client_index, buff);
  free(buff);
}





int check_for_client(int clients_num, char* buff){
  //
  //check clients array for desired client
  //
  int client_index = -1;
  for(int i = 0; i < numClients; i++){
    if(i == clients_num){continue;}
    int ret = strcmp(buff, clients[i].name);

    //client found
    if(ret == 0){
      client_index = i;
      break;
    }
  }
  return client_index;

}

void process_talk(int clients_num, int client_index, char* buff){

  //Was a client found?
  //Yes! Client found!
  if(client_index != -1){
    //
    // Client is in Talk Session already
    //
    if(clients[client_index].inCall != 0){
      write(clients[clients_num].fd, clients[client_index].name, strlen(clients[client_index].name));
      write(clients[clients_num].fd, " is in Talk session. Request Denied.\n", strlen(" is in Talk session. Request Denied.\n"));

    }else{
      //
      // Sending talk request to desired client
      //
      establish_connection(clients_num, client_index);
    }
    //
    //No clients here! Return to accepted_client
    //
  }else{
    write(clients[clients_num].fd, buff, strlen(buff));
    write(clients[clients_num].fd, NOT_LOGGED_IN, strlen(NOT_LOGGED_IN));
  }
}

void establish_connection(int clients_num, int client_index){
      //
      // Sending talk request to desired client
      //
      printf("Sending Talk Request to %s\n", clients[client_index].name);
      clients[client_index].ringing = 1;
      write(clients[client_index].fd, "Talk request from ", strlen("Talk request from "));
      write(clients[client_index].fd, clients[clients_num].name, strlen(clients[clients_num].name));
      write(clients[client_index].fd, "@", 1);
      write(clients[client_index].fd, clients[clients_num].ip, strlen(clients[clients_num].ip));
      write(clients[client_index].fd, ". Respond with \"accept ", strlen(". Respond with \"accept "));
      write(clients[client_index].fd, clients[clients_num].name, strlen(clients[clients_num].name));
      write(clients[client_index].fd, "@", 1);
      write(clients[client_index].fd, clients[clients_num].ip, strlen(clients[clients_num].ip));
      write(clients[client_index].fd, "\"\n", 2);

      //
      // Sending ring message to calling client
      //
      printf("Sending Ring Message to %s\n", clients[clients_num].name);
      write(clients[clients_num].fd, "Ringing ", strlen("ringing "));
      write(clients[clients_num].fd, clients[client_index].name, strlen(clients[client_index].name));
      write(clients[clients_num].fd, "\n", 1);


      while(clients[client_index].ringing){}
      clients[client_index].client1 = clients[clients_num].index;
      clients[clients_num].client1 = clients[client_index].index;

      converse(clients_num);

}

void accept_call(int clients_num, char* buff){
  char name[BUFFSIZE];
  char ip[BUFFSIZE];

  if(0 == strncmp(buff, "accept ", strlen("accept "))){
    int i=0, j=strlen("accept ");
    while(buff[i++] = buff[j++]){}
    i=0;
    j=0;
    while(buff[i] != '@'){
      name[j++] = buff[i++];
    }
    name[j]='\0';
    i++;
    j=0;
    while(buff[i]){
      ip[j++] = buff[i++];
    }
    ip[j] = '\0';

    buff[0] = '\n';
    buff[1] = '\0';

    for(int i = 0; i < numClients; i++){
      if(i == clients_num){continue;}
      if((0 == strcmp(name, clients[i].name)) && (0 == strcmp(ip, clients[i].ip))){
        clients[clients_num].ringing = 0;
        sleep(1);
        printf("Reciveced accept message from %s\n", clients[clients_num].name);
        printf("Sending Talk initiated message to %s\n", clients[i].name);
        write(clients[i].fd, ESTABLISHED, strlen(ESTABLISHED));
        write(clients[i].fd, clients[clients_num].name, strlen(clients[clients_num].name));
        write(clients[i].fd, "\n", 1);
        if(clients[i].inCall == 0){
          write(clients[clients_num].fd, "Waiting for one more...\n", strlen("Waiting for one more...\n"));
          write(clients[i].fd, "Waiting for one more...\n", strlen("Waiting for one more...\n"));
          while(clients[i].inCall == 0){}
          write(clients[i].fd, "Everyone's here!\n", strlen("Everyone's here!\n"));
          write(clients[clients_num].fd, "Everyone's here!\n", strlen("Everyone's here!\n"));
        }
        converse(clients_num);
        break;
      }
    }
  }
}


void converse(int clients_num){

  clients[clients_num].inCall = 1;

  char buff[BUFFSIZE];
  int i;

  //
  // For one client
  //
  if(clients[clients_num].client2 == -1){
    //
    // read client input forever (until 'exit')
    //
    while(clients[clients_num].inCall){
      read(clients[clients_num].fd, &buff, BUFFSIZE);

      if(clients[clients_num].inCall == 0){
        continue;
      }
      //
      // parsing newlines and carriage returns
      //
      for(i = 0; i < BUFFSIZE; i++){
        if(buff[i] == '\n' || buff[i] == '\r'){
          buff[i] = '\0';
          break;
        }
      }

      printf("Recieved \"%s\" from %s\n", buff, clients[clients_num].name);

      //
      // 'exit'
      //
      if(0 == strncmp(buff, EXIT, strlen(EXIT))){

        printf("Sending connection terminated messages\n");
        write(clients[clients_num].fd, TERMINATED, strlen(TERMINATED));
        write(clients[clients_num].fd, clients[clients[clients_num].client1].name, strlen(clients[clients[clients_num].client1].name));
        write(clients[clients_num].fd, "\n", 1);
        write(clients[clients[clients_num].client1].fd, TERMINATED, strlen(TERMINATED));
        write(clients[clients[clients_num].client1].fd, clients[clients_num].name, strlen(clients[clients_num].name));
        write(clients[clients[clients_num].client1].fd, "\n", 1);

        clients[clients[clients_num].client1].inCall = 0;
        clients[clients_num].inCall = 0;

        clients[clients[clients_num].client1].client1 = -1;
        clients[clients_num].client1 = -1;
        //
        // otherwise, send message
        //
      }else{
        printf("Sending message to %s\n\n", clients[clients[clients_num].client1].name);
        buff[i++]='\n';
        buff[i] = '\0';
        write(clients[clients[clients_num].client1].fd, clients[clients_num].name, strlen(clients[clients_num].name));
        write(clients[clients[clients_num].client1].fd, ": ", 2);
        write(clients[clients[clients_num].client1].fd, buff, strlen(buff));
      }
    }



  //
  // For multiple clients
  //
  }else{
    //
    // read client input forever (until 'exit')
    //
    while(clients[clients_num].inCall){
      read(clients[clients_num].fd, &buff, BUFFSIZE);

      if(clients[clients_num].inCall == 0){
        continue;
      }
      //
      // parsing newlines and carriage returns
      //
      for(i = 0; i < BUFFSIZE; i++){
        if(buff[i] == '\n' || buff[i] == '\r'){
          buff[i] = '\0';
          break;
        }
      }

      printf("Recieved \"%s\" from %s\n", buff, clients[clients_num].name);

      //
      // 'exit'
      //
      if(0 == strncmp(buff, EXIT, strlen(EXIT))){

        printf("Sending connection terminated messages\n");
        write(clients[clients_num].fd, CONF_TERMINATED, strlen(CONF_TERMINATED));
        write(clients[clients_num].fd, clients[clients_num].name, strlen(clients[clients_num].name));
        write(clients[clients_num].fd, "\n", 1);
        write(clients[clients[clients_num].client1].fd, CONF_TERMINATED, strlen(CONF_TERMINATED));
        write(clients[clients[clients_num].client1].fd, clients[clients_num].name, strlen(clients[clients_num].name));
        write(clients[clients[clients_num].client1].fd, "\n", 1);
        write(clients[clients[clients_num].client2].fd, CONF_TERMINATED, strlen(CONF_TERMINATED));
        write(clients[clients[clients_num].client2].fd, clients[clients_num].name, strlen(clients[clients_num].name));
        write(clients[clients[clients_num].client2].fd, "\n", 1);

        clients[clients[clients_num].client1].inCall = 0;
        clients[clients[clients_num].client2].inCall = 0;
        clients[clients_num].inCall = 0;

        clients[clients[clients_num].client1].client1 = -1;
        clients[clients[clients_num].client2].client1 = -1;
        clients[clients_num].client1 = -1;

        clients[clients[clients_num].client1].client2 = -1;
        clients[clients[clients_num].client2].client2 = -1;
        clients[clients_num].client2 = -1;


        //
        // otherwise, send message
        //
      }else{
        printf("Sending message to %s\n\n", clients[clients[clients_num].client1].name);
        printf("Sending message to %s\n\n", clients[clients[clients_num].client2].name);
        buff[i++]='\n';
        buff[i] = '\0';
        write(clients[clients[clients_num].client1].fd, clients[clients_num].name, strlen(clients[clients_num].name));
        write(clients[clients[clients_num].client1].fd, ": ", 2);
        write(clients[clients[clients_num].client1].fd, buff, strlen(buff));
        write(clients[clients[clients_num].client2].fd, clients[clients_num].name, strlen(clients[clients_num].name));
        write(clients[clients[clients_num].client2].fd, ": ", 2);
        write(clients[clients[clients_num].client2].fd, buff, strlen(buff));
      }
    }
  }
}


//conference talk
void conf_talk(int clients_num, char* oldBuff){
  char buff1[BUFFSIZE];
  char buff2[BUFFSIZE];

  int i = 0, comma_hit = 0, j = strlen(CONF_TALK) + 1;
  while(oldBuff[j]){
    if(oldBuff[j] == ','){
      comma_hit++;
      buff1[i] = '\0';
      i=0;
      j++;
      j++;
      continue;
    }
    if(comma_hit){
      buff2[i++] = oldBuff[j++];
    }else{
      buff1[i++] = oldBuff[j++];
    }
  }
  buff2[i] = '\0';

  int client_index1 = check_for_client(clients_num, buff1);
  int client_index2 = check_for_client(clients_num, buff2);
  process_conf_talk(clients_num, client_index1, buff1, client_index2, buff2);
}




void process_conf_talk(int clients_num, int index1, char* buff1, int index2, char* buff2){

  //Was a client found?
  //Yes! Client found!
  if(index1 != -1){
    //
    // Client is in Talk Session already
    //
    if(clients[index1].inCall != 0){
      write(clients[clients_num].fd, clients[index1].name, strlen(clients[index1].name));
      write(clients[clients_num].fd, " is in Talk session. Request Denied.\n", strlen(" is in Talk session. Request Denied.\n"));

    }else{
      //
      //Yes! Client2 being tested now.
      //
      if(index2 != -1){
        //
        // Client2 is in Talk Session already
        //
        if(clients[index2].inCall != 0){
          write(clients[clients_num].fd, clients[index2].name, strlen(clients[index2].name));
          write(clients[clients_num].fd, " is in Talk session. Request Denied.\n", strlen(" is in Talk session. Request Denied.\n"));

        }else{
          //
          // Sending talk requests to desired clients
          //
          establish_conf_connection(clients_num, index1, index2);
        }
        //
        //Client2 not here! Return to accepted_client
        //
      }else{
        write(clients[clients_num].fd, buff2, strlen(buff2));
        write(clients[clients_num].fd, NOT_LOGGED_IN, strlen(NOT_LOGGED_IN));
      }

    }
    //
    //Client1 not here! Return to accepted_client
    //
  }else{
    write(clients[clients_num].fd, buff1, strlen(buff1));
    write(clients[clients_num].fd, NOT_LOGGED_IN, strlen(NOT_LOGGED_IN));
  }
}


void establish_conf_connection(int clients_num, int index1, int index2){
      //
      // Sending talk request to client1
      //
      printf("Sending Talk Request to %s\n", clients[index1].name);
      clients[index1].ringing = 1;
      write(clients[index1].fd, "Talk request from ", strlen("Talk request from "));
      write(clients[index1].fd, clients[clients_num].name, strlen(clients[clients_num].name));
      write(clients[index1].fd, "@", 1);
      write(clients[index1].fd, clients[clients_num].ip, strlen(clients[clients_num].ip));
      write(clients[index1].fd, ". Respond with \"accept ", strlen(". Respond with \"accept "));
      write(clients[index1].fd, clients[clients_num].name, strlen(clients[clients_num].name));
      write(clients[index1].fd, "@", 1);
      write(clients[index1].fd, clients[clients_num].ip, strlen(clients[clients_num].ip));
      write(clients[index1].fd, "\"\n", 2);
      //
      // Sending talk request to client2
      //
      printf("Sending Talk Request to %s\n", clients[index2].name);
      clients[index2].ringing = 1;
      write(clients[index2].fd, "Talk request from ", strlen("Talk request from "));
      write(clients[index2].fd, clients[clients_num].name, strlen(clients[clients_num].name));
      write(clients[index2].fd, "@", 1);
      write(clients[index2].fd, clients[clients_num].ip, strlen(clients[clients_num].ip));
      write(clients[index2].fd, ". Respond with \"accept ", strlen(". Respond with \"accept "));
      write(clients[index2].fd, clients[clients_num].name, strlen(clients[clients_num].name));
      write(clients[index2].fd, "@", 1);
      write(clients[index2].fd, clients[clients_num].ip, strlen(clients[clients_num].ip));
      write(clients[index2].fd, "\"\n", 2);


      //
      // Sending ring message to calling client
      //
      printf("Sending Ring Message to %s\n", clients[clients_num].name);
      write(clients[clients_num].fd, "Ringing ", strlen("Ringing "));
      write(clients[clients_num].fd, clients[index1].name, strlen(clients[index1].name));
      write(clients[clients_num].fd, " and ", strlen(" and "));
      write(clients[clients_num].fd, clients[index2].name, strlen(clients[index2].name));
      write(clients[clients_num].fd, "\n", 1);


      while(clients[index1].ringing || clients[index2].ringing){}
      clients[index1].client1 = clients[clients_num].index;
      clients[index1].client2 = clients[index2].index;
      clients[index2].client1 = clients[clients_num].index;
      clients[index2].client2 = clients[index1].index;
      clients[clients_num].client1 = clients[index1].index;
      clients[clients_num].client2 = clients[index2].index;

      converse(clients_num);

}

