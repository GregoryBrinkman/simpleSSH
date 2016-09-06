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
#define BUFFSIZE 512
#define REMBASH "<rembash>\n"
#define SECRET "cs407rembash\n"
#define ERROR "<error>\n"
#define OK "<ok>\n"

void handle_client(int);

int main(int argc, char *argv[])
{
  int server_sockfd, client_sockfd;
  int result;
  socklen_t server_len, client_len;
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;

  if(-1 == (server_sockfd = socket(AF_INET, SOCK_STREAM, 0))){
    perror("");
    exit(1);
  }
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(PORT);
  server_len = sizeof(server_address);

  result = bind(server_sockfd, (struct sockaddr *)&server_address, server_len);
  if(-1 == result){
    perror("Bind refused");
    exit(1);
  }

  if(0 != (result = listen(server_sockfd, 5))){
    perror("");
    exit(1);
  }
  signal(SIGCHLD, SIG_IGN);

  while(1) {
    client_len = sizeof(client_address);
    client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len);
    if(-1 == client_sockfd){
      perror("");
      close(client_sockfd);
    }else
      handle_client(client_sockfd);
  }
}

void handle_client(int connect_fd){
  char buff[BUFFSIZE];

  write(connect_fd, &REMBASH, strlen(REMBASH));
  read(connect_fd, &buff, strlen(SECRET));

  if(0 != strncmp(buff, SECRET, strlen(SECRET))){
    write(connect_fd, &ERROR, strlen(ERROR));
    close(connect_fd);
    return;
  }

  write(connect_fd, &OK, strlen(OK));

  //child
  if(fork() == 0){
    int i = 0;
    while(i < 3) dup2(connect_fd, i++);
    setsid();
    execlp("bash", "bash", "--noediting", "-i", NULL);
  }
  close(connect_fd);
  return;
}
