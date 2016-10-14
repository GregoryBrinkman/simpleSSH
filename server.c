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

//constants
#define PORT 4070
#define BUFFSIZE 4096
#define REMBASH "<rembash>\n"
#define SECRET "<cs407rembash>\n"
#define ERROR "<error>\n"
#define OK "<ok>\n"
#define MAX_NUM_CLIENTS 100

#define DEBUG

//global variables
int epfd;
int fd[MAX_NUM_CLIENTS * 2 + 5];

//function declarations
void timer_handler(int, siginfo_t *, void *);
void *epoll_loop(void*);
void *handle_client(void*);
void accepted_client(int);
pid_t getpty(int*, const struct termios* /* , const struct winsize* */);

int main(int argc, char *argv[])
{

  //variable instantiation
  int server_sockfd, client_sockfd;
  int result;
  socklen_t server_len, client_len;
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;

  //epoll setup
  if((epfd = epoll_create1(SOCK_CLOEXEC)) == -1){
    perror("Epoll failure");
    exit(EXIT_FAILURE);
  }

  //socket setup
  if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    perror("Socket Failed");
    exit(EXIT_FAILURE);
  }

  //disable Nagle's algorithm
  int i = 1;
  setsockopt(server_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &i, sizeof(int));

  //set variables for socket bind to port 4070
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(PORT);
  server_len = sizeof(server_address);

  if((result = bind(server_sockfd, (struct sockaddr *)&server_address, server_len)) == -1){
    perror("Bind refused");
    exit(EXIT_FAILURE);
  }

  //creation of listening socket
  if((result = listen(server_sockfd, 5)) != 0){
    perror("Listen Failed");
    exit(EXIT_FAILURE);
  }

  signal(SIGCHLD, SIG_IGN);
  client_len = sizeof(client_address);

  pthread_t epoll_thread;
  pthread_create(&epoll_thread, NULL, epoll_loop, NULL);
  //accept loop
  while(1) {
    client_sockfd = accept4(server_sockfd, (struct sockaddr *)&client_address, &client_len, SOCK_CLOEXEC);

    if(client_sockfd != -1)
      {
        pthread_t handle_thread;
        int *fd = malloc(sizeof(int));
        *fd = client_sockfd;
        pthread_create(&handle_thread, NULL, handle_client, fd);

#ifdef DEBUG
        printf("pthread created: %ld, connect_fd is %i\n", (long) handle_thread, client_sockfd);
#endif

      }
    /* pthread_create */
    /* handle_client(client_sockfd); */
    /* close(client_sockfd); */
  }
}

void *epoll_loop(void *a){
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
        
        #ifdef DEBUG
        printf("from client: %i, from master: %i", ev[i].data.fd, fd[ev[i].data.fd]);
        #endif
        /* relay data */
        static char buff[BUFFSIZE];
        int nwrite, total, readlen;
        if((readlen = read(ev[i].data.fd, &buff, BUFFSIZE)) > 0){

          total = 0;
          do{
            if((nwrite = write(fd[ev[i].data.fd], buff+total, readlen-total)) == -1) break;
          }while((total += nwrite) < readlen);
        }
      }else if(ev[i].events & (EPOLLERR | EPOLLHUP)){
    /* close fds */
        close(fd[ev[i].data.fd]);
        close(ev[i].data.fd);
  }
}

}
}

void *handle_client(void * arg){
  
  char buff[BUFFSIZE];

  int connect_fd = *(int*)arg;
  free(arg);

  int flag = 1;

#ifdef DEBUG
  printf("connect_fd = %i\n", connect_fd);
#endif

  //signal handler
  struct sigaction act;
  act.sa_sigaction = timer_handler;
  act.sa_flags = SA_SIGINFO;

  if(sigaction(SIGRTMAX, &act, NULL) == -1) {
    perror("Signal Handler Construction Failed");
    exit(EXIT_FAILURE);
  }

  //sigev
  struct sigevent sev;
  sev.sigev_notify = SIGEV_THREAD_ID;
  sev.sigev_signo = SIGRTMAX;
  
  sev.sigev_value.sival_ptr = &flag;
  sev._sigev_un._tid = syscall(SYS_gettid);


  struct itimerspec ts;
  ts.it_value.tv_sec = 2;

  //start timer
  timer_t tid;
  timer_create(CLOCK_REALTIME, &sev, &tid);
  timer_settime(tid, 0, &ts, NULL);

  //handshake
  if(write(connect_fd, &REMBASH, strlen(REMBASH)) == -1)
    {
      close(connect_fd);
      pthread_exit(NULL);
    }

  if(flag == 0)
    {
      close(connect_fd);
      pthread_exit(NULL);
    }

  if(read(connect_fd, &buff, strlen(SECRET)) == -1)
    {
      close(connect_fd);
      pthread_exit(NULL);
    }

  if(flag == 0)
    {
      close(connect_fd);
      pthread_exit(NULL);
    }

  if(0 != strncmp(buff, SECRET, strlen(SECRET))){
    write(connect_fd, &ERROR, strlen(ERROR));
    close(connect_fd);
    pthread_exit(NULL);
  }

  write(connect_fd, &OK, strlen(OK));
  //end timer
  timer_delete(tid);
  
  accepted_client(connect_fd);
  pthread_exit(NULL);
}


void accepted_client(int connect_fd)
{
  /* struct winsize ws; */
  int masterfd;
  struct termios tty;
  struct epoll_event ev[2];

#ifdef DEBUG
  printf("accepted client\n");
#endif

  if((getpty(&masterfd, &tty/*, &ws */)) == -1)
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


  fd[masterfd] = connect_fd;
  fd[connect_fd] = masterfd;

  ev[0].data.fd = connect_fd;
  ev[1].data.fd = masterfd;
  ev[0].events = EPOLLIN | EPOLLET;
  ev[1].events = EPOLLIN | EPOLLET;
  epoll_ctl(epfd, EPOLL_CTL_ADD, connect_fd, ev);
  epoll_ctl(epfd, EPOLL_CTL_ADD, masterfd, ev + 1);

}

pid_t getpty(int *masterfd, const struct termios *tty){
  /* ,const struct winsize *ws){ */
  char* slavename;
  int mfd, slavefd;
  mfd = posix_openpt(O_RDWR|O_CLOEXEC); 

  if (mfd == -1 || grantpt(mfd) == -1 || unlockpt(mfd) == -1 || (slavename = ptsname(mfd)) == NULL){
    close(mfd);
    return -1;
  }

  //child
  pid_t pid;
  if((pid = fork()) == 0){

#ifdef DEBUG
    printf("slavename = %s\n", slavename);
#endif

    close(mfd);
    if(setsid() == -1)
      return -1;

    if((slavefd = open(slavename, O_RDWR)) < 0)
      return -1;

    /* tty->c_lflag &= ~(ICANON|IEXTEN); */

    if(tcsetattr(slavefd, TCSANOW, tty) == -1)
      return -1;


    /* if(ioctl(slavefd, TIOCSWINSZ, ws) == -1) */
    /*   return -1; */

    int i = 0;
    while(i < 3)
      if(dup2(slavefd, i) == i)
        i++;
      else
        return -1;

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

void timer_handler(int signal, siginfo_t *siginfo, void *arg)
{
#ifdef DEBUG
  printf("TIMER HIT\n");
#endif

  *(int*)siginfo->si_ptr = 0;
}

