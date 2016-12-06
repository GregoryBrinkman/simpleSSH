//test macros
#define _POSIX_C_SOURCE 199309L
#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

//header files
#include <sys/syscall.h>
#include <sys/epoll.h>
#include <errno.h>
#include <error.h>
#include <pthread.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
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
#include "tpool.h"

//constants
#define STDIN 0
#define STDOUT 1
#define STDERR 2
#define PORT 4070
#define BUFFSIZE 4096
#define REMBASH "<rembash>\n"
#define SECRET "<cs407rembash>\n"
#define ERROR "<error>\n"
#define OK "<ok>\n"
#define MAX_NUM_CLIENTS 100

#define DEBUG

typedef struct client
{
  int sock;
  int pty;
  int state;
} client_t;

//global variables
int epfd;
client_t clientArray[MAX_NUM_CLIENTS * 2 + 5];

//function declarations
int socket_init();
int epoll_init();
void *epoll_loop(void*);
void handle_event(int fd);

/* void *handle_client(void*); */
/* void accepted_client(int); */
/* pid_t getpty(int* /\*, const struct termios* , const struct winsize* *\/); */

int main(int argc, char *argv[])
{
  signal(SIGCHLD, SIG_IGN);
  int server_fd;
  if((server_fd = socket_init()) == 0) {
    fprintf(stderr, "oops! server socket couldn't initialize\n");
    exit(EXIT_FAILURE);
  }

  if(epoll_init() == 0) {
    fprintf(stderr, "oops! epoll couldn't initialize\n");
    exit(EXIT_FAILURE);
  }

  //initialize thread pool
  tpool_init(handle_event);
  /* if(tpool_init(handle_event) == 0)  { */
  /*   fprintf(stderr, "oops! tpool couldn't initialize\n"); */
  /*   exit(EXIT_FAILURE); */
  /* } */


  int client_fd;
  socklen_t client_len;
  struct sockaddr_in client_address;
  //accept loop
  while(1) {
    client_fd = accept4(server_fd, (struct sockaddr *)&client_address, &client_len, SOCK_CLOEXEC);

    if(client_fd != -1){

      struct epoll_event ev[2];
      clientArray[client_fd].sock = client_fd;
      /* fd[connect_fd] = masterfd; */

      ev[0].data.fd = client_fd;
      /* ev[1].data.fd = masterfd; */
      ev[0].events = EPOLLIN | EPOLLET;
      /* ev[1].events = EPOLLIN | EPOLLET; */
      epoll_ctl(epfd, EPOLL_CTL_ADD, clientArray[client_fd].sock, ev);
      /* epoll_ctl(epfd, EPOLL_CTL_ADD, masterfd, ev + 1); */


      printf("In clientarray at index %d: sock = %d, state = %d\n",client_fd, clientArray[client_fd].sock, clientArray[client_fd].state);

      //pthread_t handle_thread;

      // pass in file descriptor to thread
      /* int *fd = malloc(sizeof(int)); */
      /* *fd = client_fd; */

      //create thread running handle_client in detached mode
      //pthread_create(&handle_thread, &attr, handle_client, fd);
      /* if(tpool_add_task(*(int*)fd) == 0) { */
      /*   fprintf(stderr, "FD could not be added to tpool"); */
      /* } */
    }
  }
}

int epoll_init(){

  //epoll setup
  if((epfd = epoll_create1(SOCK_CLOEXEC)) == -1){
    return 0;
  }

  pthread_t epoll_thread;
  pthread_attr_t attr;
  //set thread to start in detached mode
  if(pthread_attr_init(&attr) != 0){
    perror("pthread attr");
    return 0;
  }
  if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0){
    perror("pthread attr setdetachstate");
    return 0;
  }
  if(pthread_create(&epoll_thread, &attr, epoll_loop, NULL) != 0){
    perror("pthread create");
    return 0;
  }
  return epfd;
}

int socket_init(){

  int server_sockfd;
  int result;
  socklen_t server_len;
  struct sockaddr_in server_address;

  //socket setup
  if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    return 0;
  }

  //disable Nagle's algorithm
  int i = 1;
  setsockopt(server_sockfd, IPPROTO_TCP, TCP_NODELAY,
             (char *) &i, sizeof(int));

  //set variables for socket bind to port 4070
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(PORT);
  server_len = sizeof(server_address);

  if((result = bind(server_sockfd, (struct sockaddr *)&server_address,
                    server_len)) == -1){
    return 0;
  }
  //creation of listening socket
  if((result = listen(server_sockfd, 5)) != 0){
    return 0;
  }
  return server_sockfd;
}

void *epoll_loop(void *ignored){
  struct epoll_event ev[MAX_NUM_CLIENTS];
  int events, i;

  while(1){
    events=epoll_wait(epfd, ev, MAX_NUM_CLIENTS, -1);
    printf("Epoll got event");
    if(events == -1){
      if(errno == EINTR){
        continue;
      }else{
        perror("epoll loop");
        exit(EXIT_FAILURE);
      }
    }
    for(i = 0; i < events; i++){

      if(ev[i].events & EPOLLIN){
        tpool_add_task(ev[i].data.fd);
      }else if(ev[i].events & (EPOLLERR | EPOLLHUP)){
        /* close fds */
        close(clientArray[ev[i].data.fd].sock);
        close(ev[i].data.fd);
      }
    }
  }
}


void handle_event(int fd){

  printf("In clientarray at index %d: sock = %d, state = %d\n",fd, clientArray[fd].sock, clientArray[fd].state);
  printf("in handle event");
  char buff[BUFFSIZE];

  if(clientArray[fd].state == 0){
    /* new client */
    if(write(clientArray[fd].sock, &REMBASH, strlen(REMBASH)) == -1)
        close(clientArray[fd].sock);
    else
      clientArray[fd].state = clientArray[fd].state + 1;
  }else if(clientArray[fd].state == 1){
    /* secret  */
    if(read(clientArray[fd].sock, &buff, strlen(SECRET)) == -1)
      {
        close(clientArray[fd].sock);
        pthread_exit(NULL);
      }

    if(0 != strncmp(buff, SECRET, strlen(SECRET))){
      write(clientArray[fd].sock, &ERROR, strlen(ERROR));
      close(clientArray[fd].sock);
    }

    write(clientArray[fd].sock, &OK, strlen(OK));

    clientArray[fd].state = clientArray[fd].state + 1;
    /* accepted_client(clientArray[fd].sock); */
  }else{
    /* readwrite */

    /*     int nwrite, total, readlen; */
    /*     if((readlen = read(fd, &buff, BUFFSIZE)) > 0){ */
    /*       total = 0; */
    /*       do{ */
    /*         if((nwrite = write(fd[ev[i].data.fd], buff+total, readlen-total)) == -1) break; */
    /*       }while((total += nwrite) < readlen); */
    /*     }else{ */

    /* #ifdef DEBUG */
    /*       if(readlen == 0) */
    /*         printf("Read returned 0; Interrupted client\n"); */
    /*       else */
    /*         printf("read returned -1; ERROR"); */
    /*       printf("closed client: %i, closed master: %i\n", ev[i].data.fd, fd[ev[i].data.fd]); */
    /* #endif */

    /*       //error! */
    /*       close(fd[ev[i].data.fd]); */
    /*       close(ev[i].data.fd); */
    /*     } */
  }
}
