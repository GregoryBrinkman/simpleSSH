#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define BUFFSIZE 512
#define CONFIRMATION "<confirm>\n"
#define OK "<ok>\n"

void error(const char *);

int main(int argc, char *argv[])
{
  //
  //Socket Variable Instantiation
  //
  static char buff[BUFFSIZE];
  int sockfd, len, result;
  struct sockaddr_in address;

  //
  //Check number of command line arguments
  //
  if(argc < 2 || argc > 2)
    error("USAGE: program ip-address");

  //
  //Get Command line argument
  //
  char *tmp = argv[1];

  //
  //parse ip and port from command line argument
  //
  char ip[16], port[7];
  int i = 0, j = 0, flag = 0;
  while(tmp[i] != '\0'){
    if(tmp[i] == ':'){
      ip[i] = '\0';
      flag = 1;
      i++;
      continue;
    }
    if(!flag)
      ip[i] = tmp[i];
    else
      port[j++] = tmp[i];
    i++;
  }
  port[j] = '\0';


  //
  //Set up socket struct with ip and port
  //
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr(ip);
  address.sin_port = htons(atoi(port));
  len = sizeof(address);

  //
  //Connect socket to server
  //
  result = connect(sockfd, (struct sockaddr *)&address, len);

  if(-1 == result) error("Connection refused");

  //
  //Check if server is running correct program
  //
  read(sockfd, &buff, strlen(CONFIRMATION));
  result = strncmp(buff, CONFIRMATION, strlen(CONFIRMATION));
  if(0 != result) {
    close(sockfd);
    error("Protocol Incorrect");
  }

  write(sockfd, &CONFIRMATION, strlen(CONFIRMATION));
  read(sockfd, &buff, BUFFSIZE);

  if(0 != strncmp(buff, OK, strlen(OK))){
    close(sockfd);
    error("Wrong Secret");
  }

  //
  //child process, read from stdin to socket
  //
  if(fork() == 0){
    len = read(0, &buff, BUFFSIZE);
    while(write(sockfd, &buff, len)){
      len = read(0, &buff, BUFFSIZE);
    }
    close(sockfd);
    exit(0);
  }

  //
  //parent process, read from socket to stdout
  //
  while(0 < (len = read(sockfd, &buff, BUFFSIZE))){
    write(0, &buff, len);
  }
  close(sockfd);
  return 0;
}

//
//lazy error message method 
//
void error(const char* err_msg){
  fprintf(stderr, "%s\n", err_msg);
  exit(1);
}
