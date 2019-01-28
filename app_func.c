#include "app.h"
#include "debug.h"

/*
INPUT PARAMETERS
  - arg_req: number of arguments requested to run the program
  - argc: number of arguments given when running the program
  - argv: arguments given when running the program

This function prints out how to run the program if the number of arguments 
given is not equal to the number of arguments required and returns 0.
*/
int proper_usage(int argc, char *argv[], int arg_req, char type){
  if(argc != arg_req){

    if(type == 's'){
      fprintf(stderr, "USAGE: %s [-d] <Port_number> <Application_path>\n", \
                                                                      argv[0]);
    }
    else if(type == 'c'){
      fprintf(stderr, "USAGE: %s [-d] <MIP_destination> <Port_number> " \
                                  "<File_name> <Application_path>\n", argv[0]);
    }

    return 0;
  }

  return 1;
}

/*
INPUT PARAMETERS
  - argc: number of arguments given when running the program
  - argv: arguments given when running the program

This function handles the arguments given when running the program, such as
activating debug-mode and making sure the number of arguments given meets the
requirements. -1 on incorrect running attempts.
*/
int handle_argv(int argc, char *argv[], int arg_req, char type){
  opterr = 0; //to make getopt not print error message
  int retv = getopt(argc, argv, "d");
  if(retv == 'd'){

    if(!proper_usage(argc, argv, arg_req+1, type))
      return -1;
    
    debug = 1;
  }
  else if(!proper_usage(argc, argv, arg_req, type)){
    return -1;
  }

  return 0;
}

/*
INPUT PARAMETER
  - filename: name of file

INPUT-OUTPUT PARAMETER
  - filesize: buffer to store filesize

OUTPUT PARAMETER
  - buf: buffer containing the file

This function stores the size a file in 'filesize' before storing the file in a
buffer and returning the buffer. NULL is returned if the file is too big for
sending or if an error occur. 
*/
char *load_file(char *filename, uint16_t *filesize){
  int retv;
  FILE *file;
  char *buf;
  struct stat file_stat = { 0 };

  retv = stat(filename, &file_stat);
  if(retv == -1){
    perror("get_file(): stat()");
    return NULL;
  }

  if(file_stat.st_size > 65535){
    fprintf(stderr, "File is too big for sending.\n");
    return NULL;
  }

  *filesize = file_stat.st_size;

  file = fopen(filename, "r");
  if(file == NULL){
    perror("main(): fopen()");
    return NULL;
  }

  buf = malloc(*filesize);

  retv = fread(buf, 1, *filesize, file);

  if(ferror(file)){
    free(buf);
    fprintf(stderr, "reading error occurred\n");
    return NULL;
  }

  fclose(file);

  return buf;
}

/*
INPUT PARAMETERS
  - sockfd: socket file descriptor
  - mip_addr: MIP address
  - port: Port number
  - size: size of file

OUTPUT PARAMETER
  - retv: return value

This function creates a msghdr struct and stores mip_addr, port and size in it
before sending the struct through 'sockfd'. The return value of sendmsg() is
returned.
*/
int send_fileinfo(int sockfd, uint8_t mip_addr, uint16_t port, uint16_t size){
  int retv;

  struct iovec iov[3];
  iov[0].iov_base = &mip_addr;
  iov[0].iov_len = sizeof(uint8_t);
  iov[1].iov_base = &port;
  iov[1].iov_len = sizeof(uint16_t);
  iov[2].iov_base = &size;
  iov[2].iov_len = sizeof(uint16_t);

  struct msghdr msg = { 0 };
  msg.msg_iov = iov;
  msg.msg_iovlen = 3;

  retv = sendmsg(sockfd, &msg, 0);
  if(retv == -1)
    perror("main(): sendmsg()");

  return retv;
}

/*
INPUT PARAMETERS
  - sockfd: socket file descriptor
  - file: file to be sent
  - filesize: size of file to be sent

This function sends 'file' through 'sockfd' and returns the return value of
send().
*/
int send_file(int sockfd, char *file, uint16_t filesize){
  
  int retv = send(sockfd, file, filesize, 0);
  if(retv == -1)
    perror("send_file(): send()");

  return retv;
}

// /*
// INPUT PARAMETERS
//   - sockfd: socket file descriptor connected to MIP daemon on the same host

// This function creates a msghdr struct 'msg', and fills msg with the data 
// received from sockfd. mip_addr is returned at the end of the function.
// */
// int recv_msg(int sockfd){
//   ssize_t retv;
//   char buf[BUF_SIZE] = { 0 };
//   int mip_addr;

//   struct iovec iov[2];
//   iov[0].iov_base = &mip_addr;
//   iov[0].iov_len = sizeof(mip_addr);
//   iov[1].iov_base = buf;
//   iov[1].iov_len = BUF_SIZE;

//   struct msghdr msg = { 0 };
//   msg.msg_iov = iov;
//   msg.msg_iovlen = 2;

//   retv = recvmsg(sockfd, &msg, 0);
//   if(retv == 0){
//     fprintf(stderr, "connection closed!\n");
//     close(sockfd);
//     exit(EXIT_FAILURE);
//   }
//   else if(retv == -1){
//     close(sockfd);
//     return retv;
//   }

//   fprintf(stderr, "Message received from %d: %s\n", mip_addr, buf);

//   return mip_addr;
// }
