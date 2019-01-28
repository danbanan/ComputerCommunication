#ifndef TP_H
#define TP_H

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#define WIN_SIZE 10
#define FRAG_SIZE 1492
#define TP_SIZE 4
#define FDMAX 200

typedef struct{
	uint8_t ack, resent;
	int timerfd;
	uint16_t data_size;
	char data[];
} fragment_t;

/*
VARIABLES
	- lar: last ack received
	- lfs: last frame sent
	- nof: number of fragments
*/
typedef struct{
	int lar, lfs, nof;
	fragment_t *fragments[];
} sender_t;

/*
VARIABLES
	- laf: largest acceptable frame
	- lfr: last frame received
	- nof: number of fragments
	- fragments: list of fragment structs

Based on spesifications that the biggest file a client can send is 65535 
bytes, this means that we can potentially receive 45 fragments. First 
fragment is the file size, and the following 44 fragments are parts of the 
file.
*/
typedef struct{
	int lfr, laf, nof;
	fragment_t *fragments[45];
} receiver_t;

typedef struct{
	uint8_t pl;
	uint16_t port, seqnum;
} header_t;

typedef struct{
	int fd;
	uint16_t port;
	uint8_t mip_addr, timer;
	sender_t *s_win;
	receiver_t *r_win;
} fdcontext_t;

extern int epoll_fd;
extern fdcontext_t *fd_list[FDMAX];

int proper_usage(int arg_req, int argc, char *argv[]);

int handle_argv(int argc, char *argv[]);

int empty_seat(fdcontext_t *array[], int arrlen);

void init_fdctx(fdcontext_t *fdctx, int fd, uint16_t port, uint8_t mip_addr, \
														sender_t *s_win, receiver_t *r_win, uint8_t timer);

int add_fd(fdcontext_t *fdctx, int epoll_fd, fdcontext_t *array[], int arrlen);

int get_seat(int fd, fdcontext_t *array[], int arrlen);

int recv_fileinfo(int sockfd, uint8_t *mip_addr, uint16_t *port, \
																													uint16_t *filesize);

char *recv_file(int sockfd, uint16_t filesize);

char *create_tphdr(uint16_t filesize, uint16_t port, uint8_t seqnum);

char *add_hdr(char *hdr, int hdr_size, char *file, int filesize);

int send_segment(int sockfd, uint8_t mip_addr, char *data, uint16_t data_size);

int recv_segment(int sockfd, uint8_t *mip_addr, char *buf);

void init_header(char *seg, header_t *hdr);

void new_server(fdcontext_t *fdctx);

int num_of_fragments(uint16_t filesize);

void fragment_file(sender_t *win, int nof, char *file, uint16_t filesize);

sender_t *create_sender(char *file, uint16_t filesize);

int new_client(fdcontext_t *fdctx, uint16_t filesize);

int send_window(int sockfd, fdcontext_t *fdctx, int timeout);

int timeout_event(int sockfd, fdcontext_t *fdctx, int timeout);

int app_event(fdcontext_t *fdctx, fdcontext_t *list[], int epoll_fd);

int find_port(uint16_t port, fdcontext_t *array[], int arrlen);

int receiver_check(uint16_t mip_addr, header_t *hdr);

void save_fragment(receiver_t *win, header_t *hdr, char *segment, int seg_size);

int update_receiver(receiver_t *win, uint16_t seqnum);

int in_window(header_t *hdr, sender_t *win);

int update_sender(int sockfd, fdcontext_t *fdctx, int timeout);

int move_window(int sockfd, fdcontext_t *fdctx, int timeout);

// debug functions

void print_hdr(header_t *hdr);

void print_bit_values(char *data, int num);

// cleanup functions

void cleanup_fdctx(fdcontext_t *fdctx);

void cleanup_list(fdcontext_t *array[], int arrlen);

void free_fragments(sender_t *s_win, receiver_t *r_win);

#endif