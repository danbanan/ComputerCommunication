#ifndef DAEMON_H
#define DAEMON_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/uio.h>

#define BUF_SIZE 1500
#define MIP_HDR_SIZE 4
#define MAC_SIZE 6

struct header{
  uint8_t tra;
  uint8_t dst;
  uint8_t src;
  uint16_t payload;
  uint8_t ttl;
};

struct frame{
  uint8_t dst[6];
  uint8_t src[6];
  uint16_t protocol;
  char data[];
};

struct interface{
  int sockfd;
  uint8_t mip_dst;
  uint8_t mip_src;
  uint8_t mac_dst[6];
  uint8_t mac_src[6];
  struct interface *next;
};

struct ifname{
  struct ifname *next;
  char name[];
};

struct message{
  struct message *next;
  uint8_t dst;
  uint8_t src;
  uint8_t ttl;
  int data_len;
  char data[];
};

struct data{
  struct data *next;
  uint8_t dst, src, ttl;
  uint16_t data_size;
  char datagram[];
};

extern int debug;

int proper_usage(int arg_req, int argc, char *argv[]);

int handle_args(int argc, char *argv[]);

void free_data(struct data *list);

void free_interfaces(struct interface *list);

void clean_up(fd_set master, int fdmax, struct data *data_list, \
                struct interface *arp_cache, struct interface *my_interfaces);

struct ifaddrs *get_interface_names(void);

struct ifname *add_ifname(struct ifname *ifnames, char *new_name);

void free_names(struct ifname *ifnames);

struct ifname *get_ethernet_names(struct ifaddrs *ifa_list, int *ifcount);

void get_mac_addr(int sockfd, uint8_t mac[6], char *interface_name);

void init_interface(struct interface *ifa, int sockfd, uint8_t mip_dst, \
                      uint8_t mip_src, uint8_t mac_dst[6], uint8_t mac_src[6]);

struct interface *get_interface(struct interface *list, uint8_t mip_addr);

void add_interface(struct interface *new, struct interface **list);

int recv_data(int sockfd, uint8_t *mip_addr, char *buf);

void init_data(struct data *data_ptr, uint8_t dst, uint8_t src, uint8_t ttl, \
                                            uint16_t data_len, char *datagram);

void save_data(struct data *new, struct data **list);

int storage_status(struct data *list);

void remove_data(uint8_t mip_addr, struct data **list);

int size_check(int data_size);

struct data *get_data(uint8_t mip_addr, struct data *list);

char *create_miphdr(uint8_t tra, uint8_t mip_dst, uint16_t mip_src, \
                                                    int msg_len, uint8_t ttl);

char *add_miphdr(char *mip_hdr, int hdr_size, char *msg, int msg_size);

int send_frame(struct interface *ifa, char *packet, int packet_size);

int broadcast(struct interface *my_interfaces, uint8_t mip_dst);

char *create_update(struct interface *list, int update_size);

struct frame *recv_frame(int sockfd);

int send_segment(int sockfd, uint8_t mip_addr, char *seg, int seg_size);

struct header *get_header(char *data);

char *recv_update(int sockfd, int *bytes);

int request_route(int sockfd, uint8_t mip_addr);

int recv_route(int sockfd, uint16_t *route);

/* DEBUG FUNCTIONS */

void print_names(struct ifname *ifnames);

void print_mac(uint8_t mac[6]);

void print_interface(struct interface *ifa);

void print_list(struct interface *list);

void print_data(struct data *list);

void print_miphdr(char *miphdr);

void print_status(uint8_t mac_dst[6], uint8_t mac_src[6], uint8_t mip_dst, \
                                                              uint8_t mip_src);

void print_header(struct header *mip_hdr);

void dump_frame(struct frame *frame);

void print_update(char *update, int update_size);

#endif