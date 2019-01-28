#ifndef SOCK_H
#define SOCK_H

#include <stdio.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <net/if.h>

#define ETH_P_MIP 0x88B5

void close_all(fd_set master, int fdmax);

void update_fdmax(int sockfd, int *fdmax);

int create_listenfd(char *sockpath);

int connect_socket(int sockfd, char *sockpath);

int init_connection(int sockfd, char *path);

int init_rawfd(char *interface);

#endif