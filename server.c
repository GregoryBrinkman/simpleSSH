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

struct termios tty;
void sigchld_handler(int);
void handle_client(int);
void accepted_client(int);
pid_t getpty(int*, const struct termios*/* , const struct winsize* */);

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
  int i = 1;
  setsockopt(server_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &i, sizeof(int));
  //disable Nagle's algorithm

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
  static char buff[BUFFSIZE];

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

  //signal handler
  struct sigaction act;
  act.sa_handler = sigchld_handler;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  if(sigaction(SIGCHLD, &act,NULL) == -1) {
    perror("Signal Handler Construction Failed");
    exit(EXIT_FAILURE);
  }
  pid_t cpid, pid;
  if((cpid = getpty(&masterfd, &tty/*, &ws */)) == -1)
    exit(EXIT_FAILURE);

  char buff[1];
  if((pid = fork()) == 0){
    while(1){
      if(read(connect_fd, buff, 1) <= 0) break;
      if(write(masterfd, buff, 1) <= 0) break;
    }
    exit(0);
  }
  while(1){
    if(read(masterfd, buff,1) <= 0) break;
    if(write(connect_fd, buff, 1) <= 0) break;
  }

  close(connect_fd);
  close(masterfd);
  kill(pid, SIGTERM);
  kill(cpid, SIGTERM);

  act.sa_handler = SIG_IGN;
  if(sigaction(SIGCHLD, &act, NULL) == -1)
    perror("Client: Error setting SIGCHLD to be ignored");

  exit(0);


}

pid_t getpty(int *masterfd, const struct termios *tty){
  /* ,const struct winsize *ws){ */
  char* slavename;
  pid_t pid;
  int mfd, slavefd;
  mfd = posix_openpt(O_RDWR|O_NOCTTY); 

  if (mfd == -1 || grantpt(mfd) == -1 || unlockpt(mfd) == -1 || (slavename = ptsname(mfd)) == NULL){
    close(mfd);
    return -1;
  }

  //child
  if((pid = fork()) == 0){

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

  //parent
  *masterfd = mfd;
  return pid;
}

void sigchld_handler(int signal)
{
  wait(NULL);
  exit(EXIT_FAILURE);
}
