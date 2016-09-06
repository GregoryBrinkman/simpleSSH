#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define PORT 4070
#define BUFFSIZE 512
#define REMBASH "<rembash>\n"
#define SECRET "cs407rembash\n"
#define OK "<ok>\n"
#define EXIT "exit\n"

void error(const char *);

int main(int argc, char *argv[])
{
  static char buff[BUFFSIZE];
  int sockfd, len, result;
  struct sockaddr_in address;

  if(argc < 2 || argc > 2)
    error("USAGE: program ip-address");

  char *tmp = argv[1];

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr(tmp);
  address.sin_port = htons(PORT);
  len = sizeof(address);

  result = connect(sockfd, (struct sockaddr *)&address, len);

  if(-1 == result) error("Connection refused");

  read(sockfd, &buff, strlen(REMBASH));
  result = strncmp(buff, REMBASH, strlen(REMBASH));
  if(0 != result) {
    close(sockfd);
    error("Rembash Protocol Incorrect");
  }

  write(sockfd, &SECRET, strlen(SECRET));
  read(sockfd, &buff, BUFFSIZE);

  if(0 != strncmp(buff, OK, strlen(OK))){
    close(sockfd);
    error("Wrong Secret");
  }


  //child
  if(fork() == 0){
    len = read(0, &buff, BUFFSIZE);
    while(write(sockfd, &buff, len)){
      len = read(0, &buff, BUFFSIZE);
    }
    close(sockfd);
    exit(0);
  }
  //parent
  while(0 < (len = read(sockfd, &buff, BUFFSIZE))){
    write(0, &buff, len);
  }
  close(sockfd);
  return 0;
}

void error(const char* err_msg){
  fprintf(stderr, "%s\n", err_msg);
  exit(1);
}
