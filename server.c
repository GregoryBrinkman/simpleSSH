#define _XOPEN_SOURCE 600

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

#define PORT 4070
#define BUFFSIZE 4096
#define REMBASH "<rembash>\n"
#define SECRET "<cs407rembash>\n"
#define ERROR "<error>\n"
#define OK "<ok>\n"
#define DEBUG

int i = 0;
int j = 0;
int cpid[5];

struct termios tty;
void sigchld_handler(int);
void handle_client(int);
void accepted_client(int);
pid_t getpty(int*, const struct termios* /* , const struct winsize* */);

int main(int argc, char *argv[])
{
  int server_sockfd, client_sockfd;
  int result;
  socklen_t server_len, client_len;
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;


  if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    perror("Socket Failed");
    exit(1);
  }

  //disable Nagle's algorithm
  int i = 1;
  setsockopt(server_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &i, sizeof(int));

  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(PORT);
  server_len = sizeof(server_address);

  if((result = bind(server_sockfd, (struct sockaddr *)&server_address, server_len)) == -1){
    perror("Bind refused");
    exit(1);
  }

  if((result = listen(server_sockfd, 5)) != 0){
    perror("Listen Failed");
    exit(1);
  }
  signal(SIGCHLD, SIG_IGN);

  client_len = sizeof(client_address);
  while(1) {
    client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len);

    if(client_sockfd != -1)
      if(fork() == 0)
        handle_client(client_sockfd);
    close(client_sockfd);
  }
}

void handle_client(int connect_fd){
  char buff[BUFFSIZE];
  memset(buff, 0, BUFFSIZE);

  write(connect_fd, &REMBASH, strlen(REMBASH));
  read(connect_fd, &buff, strlen(SECRET));

  if(0 != strncmp(buff, SECRET, strlen(SECRET))){
    write(connect_fd, &ERROR, strlen(ERROR));
    close(connect_fd);
    exit(EXIT_FAILURE);
  }

  write(connect_fd, &OK, strlen(OK));
  accepted_client(connect_fd);
}


void accepted_client(int connect_fd)
{
  /* struct winsize ws; */
  int masterfd;

#ifdef DEBUG
  printf("accepted client\n");
#endif

  //signal handler
  struct sigaction act;
  act.sa_handler = sigchld_handler;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  if(sigaction(SIGCHLD, &act,NULL) == -1) {
    perror("Signal Handler Construction Failed");
    exit(EXIT_FAILURE);
  }

  if((cpid[0] = getpty(&masterfd, &tty/*, &ws */)) == -1)
    exit(EXIT_FAILURE);

  char buff[BUFFSIZE];
  pid_t pid;
  if((pid = fork()) == 0){
    while(1){
      if(read(connect_fd, &buff, 1) != 1) break;
      if(write(masterfd, &buff, 1) != 1) break;

#ifdef DEBUG
      printf("Child wrote %c from sock to master\n", buff[0]);
#endif

    }
    exit(0);
  }
  cpid[1]=pid;

#ifdef DEBUG
  printf("PID of child socketreader: %d\n", (int)pid);
#endif

  while(1){
    int nwrite, total, readlen;
    nwrite = 0;
    while(nwrite != -1 && (readlen = read(masterfd, &buff, BUFFSIZE)) > 0){

#ifdef DEBUG
      printf("readlen to master:%d, lenwrote to socket:%d\n", readlen, nwrite);
#endif

      total = 0;
      do{
        if((nwrite = write(connect_fd, buff+total, readlen-total)) == -1) break;
      }while((total += nwrite) < readlen);
    }
  }

  close(connect_fd);
  close(masterfd);

  act.sa_handler = SIG_IGN;
  if(sigaction(SIGCHLD, &act, NULL) == -1)
    perror("Client: Error setting SIGCHLD to be ignored");

  return;

}

pid_t getpty(int *masterfd, const struct termios *tty){
  /* ,const struct winsize *ws){ */
  char* slavename;
  int mfd, slavefd;
  mfd = posix_openpt(O_RDWR|O_NOCTTY); 

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
    return -1;
  }

#ifdef DEBUG
  printf("slave pid = %d\n", (int)pid);
#endif

  //parent
  *masterfd = mfd;
  return pid;
}

void sigchld_handler(int signal)
{
#ifdef DEBUG
  printf("SIGNAL HIT\n");
  printf("Children to MURDER: cpid[0]=%d, cpid[1]=%d\n", cpid[0], cpid[1]);
#endif

  wait(NULL);

  int x = 0;
  while(x < 2)
    kill(cpid[x++], SIGTERM);

#ifdef DEBUG
  printf("They're dead now, you have to live with what you've done...\n");
#endif
  
  exit(EXIT_SUCCESS);
}
