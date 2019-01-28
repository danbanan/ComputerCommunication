#include "app.h"
#include "debug.h"
#include "sock.h"

int debug;

int main(int argc, char *argv[]){
  int retv, sockfd;
  uint8_t mip_dst;
  uint16_t port;
  uint16_t filesize;
  char *filename;
  char *sockpath;
  char *file;

  retv = handle_argv(argc, argv, 5, 'c');
  if(retv == -1)
    exit(EXIT_SUCCESS);

  mip_dst = strtol(argv[optind], NULL, 10);
  port = strtol(argv[optind+1], NULL, 10);
  filename = argv[optind+2];
  sockpath = argv[optind+3];

  if(debug)
    DLOG("loading file");

  file = load_file(filename, &filesize);
  if(file == NULL)
    exit(EXIT_FAILURE);

  if(debug)
    DLOG("creating socket");

  sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if(sockfd == -1){
    perror("main(): socket()");
    return -1;
  }

  if(debug)
    DLOG("connecting to MIPTP-daemon");

  retv = connect_socket(sockfd, sockpath);
  if(retv == -1){
    close(sockfd);
    free(file);
    exit(EXIT_FAILURE);
  }

  if(debug)
    DLOG("sending fileinfo to MIPTP-daemon");

  retv = send_fileinfo(sockfd, mip_dst, port, filesize);
  if(retv == -1){
    free(file);
    close(sockfd);
    exit(EXIT_SUCCESS);
  }

  if(debug)
    DLOG("sending file to MIPTP-daemon");

  retv = send_file(sockfd, file, filesize);
  if(retv == -1){
    close(sockfd);
    free(file);
    exit(EXIT_FAILURE);
  }

  // test-print
  if(debug)
    fprintf(stderr, "send_file(): number of bytes sent: %d\n", retv);
  // end

  // have a ping test here, miptp sends an ack to ping_client when it
  // receives the last ack from the server-miptp.

  // start_timer();
  // recv_ack();
  // end_timer();
  // print_timespent();

  if(debug)
    DLOG("shutting down ping-client");

  close(sockfd);
  free(file);


  return EXIT_SUCCESS;
}
