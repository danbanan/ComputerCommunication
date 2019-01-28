#ifndef APP_H
#define APP_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <inttypes.h>

#define BUF_SIZE 1500

int proper_usage(int argc, char *argv[], int arg_req, char type);

int handle_argv(int argc, char *argv[], int arg_req, char type);

char *load_file(char *filename, uint16_t *filesize);

int send_fileinfo(int sockfd, uint8_t mip_addr, uint16_t port, uint16_t size);

int send_file(int sockfd, char *file, uint16_t filesize);

#endif