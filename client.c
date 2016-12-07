#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#define PORT 4070
#define BUFFSIZE 4096
#define REMBASH "<rembash>\n"
#define SECRET "<cs407rembash>\n"
#define OK "<ok>\n"
#define EXIT "exit\n"

void error(const char *);
void sigchld_handler(int);
int ttySetup(int, struct termios *);

pid_t cpid;

int main(int argc, char *argv[])
{
  int sockfd, len;
  struct sockaddr_in address;
  static char buff[BUFFSIZE];

  if(argc != 2) error("USAGE: program IP-ADDRESS");

  char *tmp = argv[1];

  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    perror("Socket Call Failed");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_port = htons(PORT);
  inet_aton(tmp, &address.sin_addr);

  if(connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == -1){
    perror("Connection refused");
    exit(EXIT_FAILURE);
  }

  len = strlen(REMBASH);
  if(read(sockfd, &buff, len) != len){
    close(sockfd);
    error("Read Failed");
  }

  if(strcmp(buff, REMBASH) != 0){
    close(sockfd);
    error("Rembash Protocol Failed");
  }

  len = strlen(SECRET);
  if(write(sockfd, &SECRET, len) != len){
    close(sockfd);
    error("Secret Write Failed");
  }
  read(sockfd, &buff, BUFFSIZE);

  if(0 != strncmp(buff, OK, strlen(OK))){
    close(sockfd);
    error("Wrong Secret");
  }

  //signal handler
  struct sigaction act;
  act.sa_handler = sigchld_handler;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  if(sigaction(SIGCHLD, &act,NULL) == -1) {
    perror("Signal Handler Construction Failed");
    exit(EXIT_FAILURE);
  }

  //setup terminal 
  struct termios prevTerm;
  if(ttySetup(1, &prevTerm) == -1)
    {
      tcsetattr(1, TCSAFLUSH, &prevTerm);
      close(sockfd);
      error("tcsetattr broke");
    }

  /* int nwrite, total; */
  switch(cpid = fork()) {
  case -1: //error
    perror("Fork failed");
    exit(EXIT_FAILURE);
  case 0: //child: reader

    while(1){
      if(read(0, buff, 1) <= 0) break;
      if(write(sockfd, buff, 1) <= 0) break;
    }
    exit(EXIT_FAILURE);
  }

  /* parent: writer */
  int nwrite, total;
  nwrite = 0;
  while(nwrite != -1 && (len = read(sockfd, &buff, BUFFSIZE)) > 0){
    total = 0;
    do{
      if((nwrite = write(1, buff+total, len-total)) == -1) break;
    }while((total += nwrite) < len);
  }

  close(sockfd);
  tcsetattr(1, TCSAFLUSH, &prevTerm);
  act.sa_handler = SIG_IGN;
  if(sigaction(SIGCHLD, &act, NULL) == -1)
    perror("Client: Error setting SIGCHLD to be ignored");

  return 0;
}

void error(const char* err_msg)
{
  fprintf(stderr, "%s\n", err_msg);
  exit(1);
}

void sigchld_handler(int signal)
{
  wait(NULL);
  kill(cpid, SIGTERM);
  exit(EXIT_FAILURE);
}

int ttySetup(int fd, struct termios *prevTerm)
{
  struct termios t;

  if(tcgetattr(fd, &t) == -1)
    return -1;

  if(prevTerm != NULL)
  *prevTerm = t;

  t.c_lflag &= ~(ICANON|ECHO);
  //t.c_cc[VMIN] = 1;
  //t.c_cc[VTIME] = 0;

  if(tcsetattr(fd, TCSAFLUSH, &t) == -1)
    return -1;
  return 0;
}
