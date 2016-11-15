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

//global variables
int epfd;
int fd[MAX_NUM_CLIENTS * 2 + 5];

//function declarations
void timer_handler(int, siginfo_t *, void *);
void *epoll_loop(void*);
void *handle_client(void*);
void accepted_client(int);
pid_t getpty(int* /*, const struct termios* , const struct winsize* */);

int main(int argc, char *argv[])
{

  //variable instantiation
  int server_sockfd, client_sockfd;
  int result;
  socklen_t server_len, client_len;
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;
  pthread_attr_t attr;

  //set thread to start in detached mode
  if(pthread_attr_init(&attr) != 0){
    perror("pthread attr");
    exit(EXIT_FAILURE);
  }

  if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0){
    perror("pthread attr setdetachstate");
    exit(EXIT_FAILURE);
  }

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
  setsockopt(server_sockfd, IPPROTO_TCP, TCP_NODELAY,
      (char *) &i, sizeof(int));

  //set variables for socket bind to port 4070
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(PORT);
  server_len = sizeof(server_address);

  if((result = bind(server_sockfd, (struct sockaddr *)&server_address,
          server_len)) == -1){
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

      // pass in file descriptor to thread
      int *fd = malloc(sizeof(int));
      *fd = client_sockfd;

      //create thread running handle_client in detached mode
      pthread_create(&handle_thread, &attr, handle_client, fd);

#ifdef DEBUG
      printf("pthread created: %ld, connect_fd is %i\n", (long) handle_thread, client_sockfd);
#endif

    }
  }
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

#ifdef DEBUG
        printf("from client: %i, from master: %i\n", ev[i].data.fd, fd[ev[i].data.fd]);
#endif

        /* relay data */
        static char buff[BUFFSIZE];
        int nwrite, total, readlen;
        if((readlen = read(ev[i].data.fd, &buff, BUFFSIZE)) > 0){
          total = 0;
          do{
            if((nwrite = write(fd[ev[i].data.fd], buff+total, readlen-total)) == -1) break;
          }while((total += nwrite) < readlen);
        }else{

#ifdef DEBUG
          if(readlen == 0)
            printf("Read returned 0; Interrupted client\n");
          else
            printf("read returned -1; ERROR");
          printf("closed client: %i, closed master: %i\n", ev[i].data.fd, fd[ev[i].data.fd]);
#endif

          //error!
          close(fd[ev[i].data.fd]);
          close(ev[i].data.fd);
        }
      }else if(ev[i].events & (EPOLLERR | EPOLLHUP)){
        /* close fds */

#ifdef DEBUG
        printf("closed client: %i, closed master: %i\n", ev[i].data.fd, fd[ev[i].data.fd]);
#endif

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

  int timer_flag = 1;

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

  sev.sigev_value.sival_ptr = &timer_flag;
  sev._sigev_un._tid = syscall(SYS_gettid);

  //set handshake timer to disconnect after 5 seconds
  struct itimerspec ts;
  ts.it_value.tv_sec = 5;

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

  if(timer_flag == 0)
  {
    close(connect_fd);
    pthread_exit(NULL);
  }

  if(read(connect_fd, &buff, strlen(SECRET)) == -1)
  {
    close(connect_fd);
    pthread_exit(NULL);
  }

  if(timer_flag == 0)
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


  fd[masterfd] = connect_fd;
  fd[connect_fd] = masterfd;

  ev[0].data.fd = connect_fd;
  ev[1].data.fd = masterfd;
  ev[0].events = EPOLLIN | EPOLLET;
  ev[1].events = EPOLLIN | EPOLLET;
  epoll_ctl(epfd, EPOLL_CTL_ADD, connect_fd, ev);
  epoll_ctl(epfd, EPOLL_CTL_ADD, masterfd, ev + 1);

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


    tty.c_lflag &= (~ECHO|ECHOK|ECHOE|~ICANON|~ECHOCTL);

    tty.c_iflag &= ~ICRNL;

    tty.c_oflag &= OPOST;

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

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

void timer_handler(int signal, siginfo_t *siginfo, void *arg)
{
#ifdef DEBUG
  printf("TIMER HIT\n");
#endif

  *(int*)siginfo->si_ptr = 0;
}

