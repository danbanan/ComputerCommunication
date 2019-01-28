#include "app.h"
#include "debug.h"
#include "sock.h"

int debug;

int main(int argc, char *argv[]){
  int retv, sockfd, running = 1;
  uint16_t port;
  char *sockpath;

  retv = handle_argv(argc, argv, 3, 's');
  if(retv == -1)
    exit(EXIT_SUCCESS);

  port = strtol(argv[optind], NULL, 10);
  sockpath = argv[optind+1];

  sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if(sockfd == -1){
    perror("main(): socket()");
    exit(EXIT_FAILURE);
  }

  DLOG("connecting to MIP-TP daemon");
  retv = connect_socket(sockfd, sockpath);
  if(retv == -1){
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  DLOG("sending fileinfo to MIP-TP daemon");
  retv = send_fileinfo(sockfd, 0, port, 0);
  if(retv == -1){
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  while(running){
    sleep(10);
    DLOG("still going...");
  }

  // recv file size
  // for(i<nof)
  // recv_segment()
  // create file
  // end

  return 0;
}
