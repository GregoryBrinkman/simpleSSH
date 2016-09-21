#define _XOPEN_SOURCE 600

#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 4070
#define BUFFSIZE 4096
#define REMBASH "<rembash>\n"
#define SECRET "<cs407rembash>\n"
#define ERROR "<error>\n"
#define OK "<ok>\n"

struct termios tty;
void handle_client(int);
void accepted_client(int);
pid_t getpty(int*, const struct termios*, const struct winsize*);

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
  /* int i = 0; */
  /* while(i < 3) dup2(connect_fd, i++); */
  /* execlp("bash", "bash", NULL); */

  struct winsize ws;
  
  //if this doesn't work, have info sent through socket or something?
  //grab terminal info from socket

  /* if(tcgetattr(connect_fd, &tty) == -1){ */
  /*   perror("tcgetattr"); */
  /*   close(connect_fd); */
  /*   exit(EXIT_FAILURE); */
  /* } */
  /* if(ioctl(connect_fd, TIOCGWINSZ, &ws) < 0){ */
  /*   perror("Error determining window size"); */
  /*   close(connect_fd); */
  /*   exit(EXIT_FAILURE); */
  /* } */

  int masterfd;

  if(getpty(&masterfd, &tty, &ws) == -1)
    exit(EXIT_FAILURE);

  /* dup2(masterfd, connect_fd); */
  /* while(1){ */
  /* } */

  ttySetRaw(&tty);


}

pid_t getpty(int *masterfd, const struct termios *tty,
             const struct winsize *ws){
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

    if(ioctl(slavefd, TIOCSWINSZ, ws) == -1)
      return -1;

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
