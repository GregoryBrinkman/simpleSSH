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

#define _XOPEN_SOURCE 600
#define PORT 4070
#define BUFFSIZE 4096
#define REMBASH "<rembash>\n"
#define SECRET "<cs407rembash>\n"
#define ERROR "<error>\n"
#define OK "<ok>\n"

void handle_client(int);
void accepted_client(int);

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
      if(fork() == 0) handle_client(client_sockfd);
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
  int masterfd, slavefd;
  char *slavename;

  masterfd = posix_openpt(O_RDWR|O_NOCTTY); 

  if (masterfd == -1 || grantpt(masterfd) == -1
      || unlockpt(masterfd) == -1 || (slavename = ptsname(masterfd)) == NULL){
    close(masterfd);
    exit(EXIT_FAILURE);
  }
  if(fork() == 0){
    close(masterfd);
    if(setsid() == -1)
      exit(EXIT_FAILURE);
    if((slavefd = open(slavename, O_RDWR)) < 0)
      exit(EXIT_FAILURE);
  }

}
