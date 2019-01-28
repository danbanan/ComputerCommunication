#include "sock.h"

/*
INPUT PARAMETER
  - fdmax: highest file descriptor number in fd_set master

INPUT-OUTPUT PARAMETER
  - master: a file descriptor set

This function closes every file descriptor in 'master'.
*/
void close_all(fd_set master, int fdmax){
  int i;

  for(i=0; i<fdmax; i++){
    if(FD_ISSET(i, &master))
      close(i);
  }

}

/*
INPUT PARAMETER
  - sockfd: socket file descriptor

INPUT-OUTPUT PARAMETER
  - fdmax: highest file descriptor number in a file descriptor set

This function compares the value of 'sockfd' to the value of 'fdmax', and updates
'fdmax' if 'sockfd' is bigger. 
*/
void update_fdmax(int sockfd, int *fdmax){
  if(sockfd > *fdmax){
    *fdmax = sockfd;
  }
}

/*
INPUT PARAMETERS
  - sockpath: socket path

OUTPUT PARAMETER
  - sockfd: socket file descriptor

This function creates a listening socket with and returns it. -1 is return if 
an error occur.
*/
int create_listenfd(char *sockpath){
  int retv, sockfd;
  struct sockaddr_un sockaddr = { 0 };

  sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if(sockfd == -1){
    perror("create_listenfd(): socket()");
    return -1;
  }

  sockaddr.sun_family = AF_UNIX;
  memcpy(sockaddr.sun_path, sockpath, strlen(sockpath));

  retv = bind(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
  if(retv == -1){
    perror("init_unixfd(): bind()");
    close(sockfd);
    return -1;
  }

  retv = listen(sockfd, 20);
  if(retv == -1){
    perror("init_unixfd(): listen()");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

/*
INPUT PARAMETER
  - sockpath: socket path

INPUT-OUTPUT PARAMETER
  - sockfd

OUTPUT PARAMETER
  - retv: return value

This function connects sockfd to a socket with a socket path identical to 
'sockpath'. The return value of connect() is returned.
*/
int connect_socket(int sockfd, char *sockpath){
  int retv;
  struct sockaddr_un my_addr = { 0 };

  my_addr.sun_family = AF_UNIX;
  memcpy(my_addr.sun_path, sockpath, strlen(sockpath));

  retv = connect(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr));
  if(retv == -1){
    perror("main(): connect()");
  }

  return retv;
}

/*
INPUT PARAMETER
  - sockfd: socket file descriptor 
  - sockpath: socket path

OUTPUT PARAMETER
  - newfd: new socket file descriptor

This function accepts a connection request from 'sockfd' and creates a new 
socket file descriptor. 'newfd' is returned.
*/
int init_connection(int sockfd, char *sockpath){
  int newfd;
  struct sockaddr_un new_addr = { 0 };
  socklen_t addrlen = sizeof(new_addr);

  new_addr.sun_family = AF_UNIX;
  memcpy(new_addr.sun_path, sockpath, strlen(sockpath));

  newfd = accept(sockfd, (struct sockaddr *)&new_addr, &addrlen);
  if(newfd == -1){
    perror("main(): accept()");
  }

  return newfd;
}

/*
Input parameter:
  - interface: name of interface

This function initializes a raw socket with the interface passed as a parameter.
The raw socket is also configured to receive frames with a broadcast address.

Return value is the initialized socket
*/
int init_rawfd(char *interface){
  int sockfd;
  int protocol = htons(ETH_P_MIP);
  int broadcast = 1;

  sockfd = socket(AF_PACKET, SOCK_RAW, protocol);
  if(sockfd == -1){
    perror("init_rawfd(): sock()");
    return -1;
  }

  struct sockaddr_ll sockaddr = { 0 };
  sockaddr.sll_family = AF_PACKET;
  sockaddr.sll_ifindex = if_nametoindex(interface);

  int retv = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, \
                    sizeof(broadcast));
  if(retv == -1){
    perror("init_raw(): setsockopt()");
    return -1;
  }

  //valgrind points to unitialized bytes
  retv = bind(sockfd, (struct sockaddr *) &sockaddr, \
                                                  sizeof(struct sockaddr_ll)); 
  if(retv == -1){
    perror("init_rawfd(): bind()");
    close(sockfd);
    return -1;
  }

  return sockfd;
}