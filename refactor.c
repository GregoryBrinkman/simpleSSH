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
void accepted_client(int);
pid_t getpty(int* /*, const struct termios* , const struct winsize* */);

int main(int argc, char *argv[])
{
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


  if (signal(SIGCHLD, SIG_IGN) == SIG_ERR){
      perror("Failed to set SIGCHLD to SIG_IGN");
      exit(EXIT_FAILURE);
    }

  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR){
      perror("Failed to set SIGPIPE to SIG_IGN");
      exit(EXIT_FAILURE);
    }

  int client_fd;
  //accept loop
  while(1) {
    socklen_t client_len;
    struct sockaddr_in client_address;
    client_fd = accept4(server_fd, (struct sockaddr *)&client_address, &client_len, SOCK_CLOEXEC);

    if(client_fd != -1){

      struct epoll_event ev[1];
      clientArray[client_fd].sock = client_fd;
      clientArray[client_fd].state = 0;
      /* fd[connect_fd] = masterfd; */

      ev[0].data.fd = client_fd;
      /* ev[1].data.fd = masterfd; */
      ev[0].events = EPOLLIN | EPOLLET;
      /* ev[1].events = EPOLLIN | EPOLLET; */
      epoll_ctl(epfd, EPOLL_CTL_ADD, clientArray[client_fd].sock, ev);
      /* epoll_ctl(epfd, EPOLL_CTL_ADD, masterfd, ev + 1); */


      /* printf("In clientarray at index %d: sock = %d, state = %d\n",client_fd, clientArray[client_fd].sock, clientArray[client_fd].state); */

      if(write(clientArray[client_fd].sock, REMBASH, strlen(REMBASH)) == -1)
        close(clientArray[client_fd].sock);

    }
  }
}

int epoll_init(){

  //epoll setup
  if((epfd = epoll_create1(EPOLL_CLOEXEC)) == -1){
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
      }else if(ev[i].events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)){
        /* close fds */
        close(clientArray[ev[i].data.fd].sock);
        close(clientArray[ev[i].data.fd].pty);
        clientArray[ev[i].data.fd].state = -1;
      }
    }
  }
}


void handle_event(int fd){

  printf("In clientarray at index %d: sock = %d, state = %d\n",fd, clientArray[fd].sock, clientArray[fd].state);
  printf("in handle event");
  char buff[BUFFSIZE];

  /* secret  */
  if(clientArray[fd].state == 0){
    if(read(clientArray[fd].sock, buff, strlen(SECRET)) == -1)
      {
        close(clientArray[fd].sock);
        clientArray[fd].state = -1;
        pthread_exit(NULL);
      }

    if(0 != strncmp(buff, SECRET, strlen(SECRET))){
      write(clientArray[fd].sock, ERROR, strlen(ERROR));
      clientArray[fd].state = -1;
      close(clientArray[fd].sock);
    }

    write(clientArray[fd].sock, OK, strlen(OK));

    clientArray[fd].state = clientArray[fd].state + 1;
    accepted_client(clientArray[fd].sock);
  }else{
    /* readwrite */
    if(fd == clientArray[fd].sock){
      printf("Reading from %d to %d\n", clientArray[fd].sock, clientArray[fd].pty);
      int nwrite, total, readlen;
      if((readlen = read(fd, buff, BUFFSIZE)) > 0){
        total = 0;
        do{
          if((nwrite = write(clientArray[fd].pty, buff+total, readlen-total)) == -1) break;
        }while((total += nwrite) < readlen);
      }else{

#ifdef DEBUG
        if(readlen == 0)
          printf("Read returned 0; Interrupted client\n");
        else
          printf("read returned -1; ERROR");
        printf("closed client: %i, closed master: %i\n", clientArray[fd].sock, clientArray[fd].pty);
#endif

        //error!
        close(clientArray[clientArray[fd].pty].sock);
        close(clientArray[clientArray[fd].pty].pty);
        clientArray[clientArray[fd].pty].state = -1;
        close(clientArray[fd].sock);
        close(clientArray[fd].pty);
        clientArray[fd].state = -1;
      }
    }
    if(fd == clientArray[fd].pty){
      printf("Reading from %d to %d\n", clientArray[fd].pty, clientArray[fd].sock);
      int nwrite, total, readlen;
      if((readlen = read(fd, buff, BUFFSIZE)) > 0){
        total = 0;
        do{
          if((nwrite = write(clientArray[fd].sock, buff+total, readlen-total)) == -1) break;
        }while((total += nwrite) < readlen);
      }else{

#ifdef DEBUG
        if(readlen == 0)
          printf("Read returned 0; Interrupted client\n");
        else
          printf("read returned -1; ERROR");
        printf("closed client: %i, closed master: %i\n", clientArray[fd].sock, clientArray[fd].pty);
#endif

        //error!
        close(clientArray[clientArray[fd].sock].sock);
        close(clientArray[clientArray[fd].sock].pty);
        clientArray[clientArray[fd].sock].state = -1;
        close(clientArray[fd].sock);
        close(clientArray[fd].pty);
        clientArray[fd].state = -1;
      }
    }
  }
}



void accepted_client(int connect_fd)
{
  /* struct winsize ws; */
  int masterfd;
  struct epoll_event ev[2];

#ifdef DEBUG
  printf("accepted client\n");
#endif

  if((getpty(&masterfd/*, &tty, &ws */)) == -1)
    exit(EXIT_FAILURE);
#ifdef DEBUG
  printf("At masterSocket epoll handoff");
#endif

  int flags;
  if((flags = fcntl(masterfd, F_GETFL, 0)) == -1){
    perror("flag error");
  }
  flags |=O_NONBLOCK;
  if((fcntl(masterfd, F_SETFL, flags)) == -1){
    perror("flag error");
  }

  clientArray[connect_fd].sock = connect_fd;
  clientArray[connect_fd].pty = masterfd;
  clientArray[masterfd].sock = connect_fd;
  clientArray[masterfd].pty = masterfd;
  clientArray[masterfd].state = 2;

  ev[0].data.fd = clientArray[connect_fd].sock;
  ev[1].data.fd = clientArray[connect_fd].pty;
  ev[0].events = EPOLLIN | EPOLLET;
  ev[1].events = EPOLLIN | EPOLLET;
  epoll_ctl(epfd, EPOLL_CTL_ADD, clientArray[connect_fd].sock, ev);
  epoll_ctl(epfd, EPOLL_CTL_ADD, clientArray[connect_fd].pty, ev + 1);

}

pid_t getpty(int *masterfd){
  /* , const struct termios *ttyOrigin){ */
  /* ,const struct winsize *ws){ */

  struct termios tty;
  char* slavename;
  int mfd, slavefd;
  mfd = posix_openpt(O_RDWR|O_CLOEXEC);

  if (mfd == -1 || grantpt(mfd) == -1 || unlockpt(mfd) == -1 ||
      (slavename = ptsname(mfd)) == NULL){
    close(mfd);
    return -1;
  }

  //child
  pid_t pid;
  if((pid = fork()) == 0){

#ifdef DEBUG
    printf("slavename = %s\n", slavename);
#endif

    //bash doesn't need this
    close(mfd);

    //set session for pseudoterminal
    if(setsid() == -1)
      return -1;

    if((slavefd = open(slavename, O_RDWR)) < 0)
      return -1;


    if(tcsetattr(slavefd, TCSAFLUSH, &tty) == -1)
      return -1;

    /* if(ioctl(slavefd, TIOCSWINSZ, ws) == -1) */
    /*   return -1; */

    //redirect I/O to PTY slave
    if(dup2(slavefd, STDIN) != STDIN)
      return -1;

    if(dup2(slavefd, STDOUT) != STDOUT)
      return -1;

    if(dup2(slavefd, STDERR) != STDERR)
      return -1;

    //execute bash for client
    execlp("bash", "bash", NULL);

    //something went wrong if we're here
    return 0;
  }

#ifdef DEBUG
  printf("HIT\n");
#endif
  //parent
  *masterfd = mfd;
  return pid;
}

