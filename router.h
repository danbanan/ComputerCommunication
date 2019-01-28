#ifndef ROUTER_H
#define ROUTER_H

#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>

#define MIP_HDR_SIZE 4
#define BUF_SIZE 1500

struct route{
	uint8_t mip_end;
	uint8_t cost;
	uint8_t mip_next;
	struct route *next;
};

int proper_usage(int arg_req, int argc, char *argv[]);

int handle_argv(int argc, char *argv[]);

void sighandler(int signum);

void free_routes(struct route *list);

void close_all(fd_set *master, int fdmax);

int new_socket(char *sockpath);

int new_fdmax(int sockfd, int fdmax);

struct route *add_route(struct route *list, struct route *new);

struct route *remove_route(struct route *list, uint8_t mip_addr);

void print_route(struct route *list);

int get_length(struct route *list, int mip_addr);

char *create_update(struct route *list, int mip_addr, int *update_size);

void print_update(char *update, int update_size);

int send_update(int sockfd, char *update, int update_size);

char *recv_update(int sockfd, int *update_size);

struct route *update_table(struct route *table, char *update, int update_size);

int recv_request(int sockfd, uint8_t *buf);

uint16_t get_next(struct route *list, uint8_t mip_req);

int send_next(int sockfd, uint16_t next);

int timeout(time_t tv_sec);

char *create_poison(struct route *table, uint8_t mip_addr, int *size);

#endif