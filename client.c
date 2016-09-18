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

#define DEBUG

void error(const char *);
void sigchld_handler(int);

int main(int argc, char *argv[])
{
  int sockfd, len;
  struct sockaddr_in address;
  static char buff[BUFFSIZE];

#ifdef DEBUG
  int i;
  i=1;
  printf("%d\n", i++);
#endif

  if(argc != 2) error("USAGE: program IP-ADDRESS");

  char *tmp = argv[1];

  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    perror("Socket Call Failed");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_port = htons(PORT);
  inet_aton(tmp, &address.sin_addr);

#ifdef DEBUG
  printf("%d\n", i++);
#endif

  if(connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == -1){
    perror("Connection refused");
    exit(EXIT_FAILURE);
  }

#ifdef DEBUG
  printf("%d\n", i++);
#endif


  len = strlen(REMBASH);
  if(read(sockfd, &buff, len) != len){
    close(sockfd);
    error("Read Failed");
  }

#ifdef DEBUG
  printf("%d\n", i++);
#endif


  if(strcmp(buff, REMBASH) != 0){
    close(sockfd);
    error("Rembash Protocol Failed");
  }

#ifdef DEBUG
  printf("%d\n", i++);
#endif


  len = strlen(SECRET);
  if(write(sockfd, &SECRET, len) != len){
    close(sockfd);
    error("Secret Write Failed");
  }
  read(sockfd, &buff, BUFFSIZE);

#ifdef DEBUG
  printf("%d\n", i++);
#endif


  if(0 != strncmp(buff, OK, strlen(OK))){
    close(sockfd);
    error("Wrong Secret");
  }

#ifdef DEBUG
  printf("%d\n", i++);
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
  

#ifdef DEBUG
  printf("%d\n", i++);
#endif

  int cpid, nwrite, total;
  switch(cpid = fork()) {
  case -1: //error
    perror("Fork failed");
    exit(EXIT_FAILURE);
  case 0: //child: reader

#ifdef DEBUG
    printf("%d\n", i++);
#endif

    nwrite = 0;
    while(nwrite != -1 && (len = read(0, &buff, BUFFSIZE)) > 0){
      total = 0;
      do{
        if((nwrite = write(sockfd, buff+total, len-total)) == -1) break;
      }while((total += nwrite) < len);
    }
    if(errno)
      perror("Client: Error during read/write from stdin to shell");
    else
      fprintf(stderr, "Client: Connection closed prematurely");
    close(sockfd);
    exit(EXIT_FAILURE);
  }
  //parent: writer
  nwrite = 0;
  while(nwrite != -1 && (len = read(sockfd, &buff, BUFFSIZE)) > 0){
    total = 0;
    do{
      if((nwrite = write(1, buff+total, len-total)) == -1) break;
    }while((total += nwrite) < len);
  }
  close(sockfd);
  act.sa_handler = SIG_IGN;
  if(sigaction(SIGCHLD, &act, NULL) == -1)
    perror("Client: Error setting SIGCHLD to be ignored");
  kill(cpid, SIGTERM);

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
  exit(EXIT_FAILURE);
}
