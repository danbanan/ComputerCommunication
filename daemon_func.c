#include "debug.h"
#include "sock.h"
#include "daemon.h"

/*
INPUT PARAMETERS
  - arg_req: number of arguments requested to run the program
  - argc: number of arguments given when running the program
  - argv: arguments given when running the program

This function prints out how to run the program if the number of arguments 
given is not equal to the number of arguments required and 0 is returned.
*/
int proper_usage(int arg_req, int argc, char *argv[]){
  if(argc < arg_req){
    fprintf(stderr, "USAGE: %s [-d] <Transport_socket> <Forwarding_socket>" \
                            " <Routing_socket> <MIP_addresses...>\n", argv[0]);
    return 0;
  }

  return 1;
}

/*
INPUT PARAMETERS
  - argc: number of arguments in cmd-line
  - argv: array of arguments in cmd-line

OUTPUT PARAMETER
  - optind: index of the next argv argument for a subsequent call of getopt()
  - debug: debug-print switch

This function handles option flags in the cmd-line and makes sure that the user
starts the program correctly. -d flag activates debug mode. 0 is returned upon
incorrect usage. 
*/
int handle_args(int argc, char *argv[]){
  opterr = 0; //to make getopt not print error message
  int retv = getopt(argc, argv, "d");
  if(retv == 'd'){

    if(!proper_usage(6, argc, argv))
      return -1;
    
    debug = 1;
  }
  else if(!proper_usage(5, argc, argv)){
    return -1;
  }

  return 0;
}

/*
INPUT-OUTPUT PARAMETER
  - list: linked list of data structs

This function call frees node of linked list.
*/
void free_data(struct data *list){
  struct data *temp;

  while(list != NULL){
    temp = list;
    list = list->next;
    free(temp);
  }
}

/*
INPUT-OUTPUT PARAMETER
  - list: linked list of interface structs

This function frees every node in the linked list.
*/
void free_interfaces(struct interface *list){
  struct interface *temp;

  while(list != NULL){
    temp = list;
    list = list->next;
    free(temp); //valgrind unitialized values
  }
}

/*
INPUT-OUTPUT PARAMETERS

  - master: fd_set
  - fdmax: highest file descriptor number of a fd_set
  - data_list: linked list of segments to be sent
  - arp_cache: linked list of interfaces of direct neighbors
  - my_interfaces: linked list of the hosts interfaces

This function performs a full clean-up of linked lists and fd_set used 
through-out main.
*/
void clean_up(fd_set master, int fdmax, struct data *data_list, \
                struct interface *arp_cache, struct interface *my_interfaces){
  close_all(master, fdmax);
  free_data(data_list);
  free_interfaces(arp_cache);
  free_interfaces(my_interfaces);
}

/*
OUTPUT PARAMETER
  - ifaddr: linked list of every interfaces on host

This function gets and returns a linked list of every interface on host. NULL 
is returned if an error occur.
  */
struct ifaddrs *get_interface_names(void){
  int retv;
  struct ifaddrs *ifaddr;

  retv = getifaddrs(&ifaddr);
  if(retv == -1){
    perror("get_interface(): getifaddrs()");
    return NULL;
  }

  return ifaddr;
}

/*
INPUT PARAMETER
  - new_name: interface name

INPUT-OUTPUT PARAMETER
  - ifnames: linked list of ifname structs

This function adds a new ifname struct to linked list ifnames or creates a new 
linked list if ifnames is NULL. 'new_name' is added to the name variable in the
new struct. The updated linked list is returned after adding a new name. 
*/
struct ifname *add_ifname(struct ifname *ifnames, char *new_name){
  struct ifname *temp = ifnames;

  if(temp != NULL){
    while(temp->next != NULL){
      temp = temp->next;
    }
    temp->next = malloc(sizeof(struct ifname) + strlen(new_name) + 1);
    memset(temp->next, 0, sizeof(struct ifname) + strlen(new_name+1));
    temp = temp->next;

    memcpy(temp->name, new_name, strlen(new_name));
    temp->next = NULL;

    return ifnames;
  }
  temp = malloc(sizeof(struct ifname)+strlen(new_name)+1);
  memset(temp, 0, sizeof(struct ifname)+strlen(new_name)+1);

  memcpy(temp->name, new_name, strlen(new_name));
  temp->next = NULL;  

  return temp;
}

/*
INPUT-OUTPUT PARAMETER
  - ifnames: linked list of interface names

This function frees every node in linked list ifnames.
*/
void free_names(struct ifname *ifnames){
  struct ifname *temp;

  while(ifnames != NULL){
    temp = ifnames;
    ifnames = ifnames->next;
    free(temp);
  }
}

/*
INPUT PARAMETER
  - ifa_list: linked list of every interface on host

INPUT-OUTPUT PARAMETER
  - ifcount: number of ethernet names buffer

OUTPUT PARAMETER
  - ifnames: linked list of every ethernet interface name on host

This function gets every ethernet interface name on host and stores it in a
linked list before returning the list.
*/
struct ifname *get_ethernet_names(struct ifaddrs *ifa_list, int *ifcount){
  int count = 0;
  struct ifname *ifnames = NULL;
  struct ifaddrs *temp = ifa_list;

  while(temp != NULL){
    struct sockaddr *addr = temp->ifa_addr;
    //assuming all ethernet intefaces has an "eth" needle in their name
    if(strstr(temp->ifa_name, "eth") != NULL){
      switch(addr->sa_family){
        case AF_PACKET:
          ifnames = add_ifname(ifnames, temp->ifa_name);
          count++;
          break;
      }
    }
    temp = temp->ifa_next;
  }
  *ifcount = count;

  return ifnames;
}

/*
INPUT PARAMETERS:
  - sockfd: socket file descriptor
  - interface_name: name of the interface

INPUT-OUTPUT PARAMETER
  - mac[6]: empty mac address

This function gets the hardware address linked to sockfd and interface_name
and stores it in the empty mac address.
*/
void get_mac_addr(int sockfd, uint8_t mac[6], char *interface_name){
  int retv;
  struct ifreq dev;
  memset(&dev, 0, sizeof(dev));
  memcpy(dev.ifr_name, interface_name, strlen(interface_name));

  retv = ioctl(sockfd, SIOCGIFHWADDR, &dev);
  if(retv == -1){
    perror("get_mac_addr(): ioctl()");
    exit(EXIT_FAILURE);
  }

  memcpy(mac, dev.ifr_hwaddr.sa_data, sizeof(dev.ifr_hwaddr.sa_data));
}

/*
INPUT PARAMETERS
  - sockfd: socket file descriptor
  - mip_dst: MIP destination address
  - mip_src: MIP source address
  - mac_dst: MAC destination address
  - mac_src: MAC source address

INPUT-OUTPUT PARAMETER
  - ifa: interface

This function initialises interface struct 'ifa'.
*/
void init_interface(struct interface *ifa, int sockfd, uint8_t mip_dst, \
                      uint8_t mip_src, uint8_t mac_dst[6], uint8_t mac_src[6]){
  ifa->sockfd = sockfd;
  ifa->mip_dst = mip_dst;
  ifa->mip_src = mip_src;
  memcpy(ifa->mac_dst, mac_dst, MAC_SIZE);
  memcpy(ifa->mac_src, mac_src, MAC_SIZE);
  ifa->next = NULL;
}

/*
INPUT PARAMETER
  - list: linked list of interfaces
  - mip_addr: MIP address

This function returns an interfaces with a MIP destinatin address identical to 
'mip_addr'. NULL is returned if such an interface does not exist in 'list'.
*/
struct interface *get_interface(struct interface *list, uint8_t mip_addr){
  struct interface *temp = list;
  while(temp != NULL){
    if(temp->mip_dst == mip_addr){
      return temp;
    }
    temp = temp->next;
  }

  return NULL;
}

/*
INPUT PARAMETER
  - new: an interface struct

INPUT-OUTPUT PARAMETER
  - list: linked list of interface structs

This function adds interface 'new' to the end of 'list'.
*/
void add_interface(struct interface *new, struct interface **list){
  struct interface *temp = *list;

  if(temp == NULL){
    *list = new;
  }
  else{

    while(temp->next != NULL){
      temp = temp->next;
    }

    temp->next = new;
  }

}

/*
INPUT PARAMETER
  - sockfd: socket file descriptor where the message is received from

INPUT-OUTPUT PARAMETERS
  - mip_addr: MIP address receive buffer
  - data: data receive buffer

OUTPUT PARAMETER
  - retv: number of bytes received/error value/connection closed value

This function receives data and a MIP address from 'sockfd' and stores it in
'mip_addr' and 'data'. The return value if recvmsg is returned.
*/
int recv_data(int sockfd, uint8_t *mip_addr, char *data){
  int retv;

  struct iovec iov[2];
  iov[0].iov_base = mip_addr;
  iov[0].iov_len = sizeof(uint8_t);
  iov[1].iov_base = data;
  iov[1].iov_len = BUF_SIZE;

  struct msghdr msg = { 0 };
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;

  retv = recvmsg(sockfd, &msg, 0);
  if(retv == 0){
    fprintf(stderr, "Connection closed!\n");
  }
  else if(retv == -1){
    perror("recv_data(): recvmsg()");
  }

  return retv;
}

/*
INPUT PARAMETERS
  - dst: MIP destination address
  - src: MIP source address
  - ttl: Time-to-live
  - data_size: size of datagram
  - datagram: data

INPUT-OUTPUT PARAMETER
  - data_ptr: data struct

This function initialises a data struct.
*/
void init_data(struct data *data_ptr, uint8_t dst, uint8_t src, uint8_t ttl, \
                                            uint16_t data_size, char *datagram){
  data_ptr->next = NULL;
  data_ptr->dst = dst;
  data_ptr->src = src;
  data_ptr->ttl = ttl;
  data_ptr->data_size = data_size;
  memcpy(data_ptr->datagram, datagram, data_size);
}

/*
INPUT PARAMETER
  - new: data struct

INPUT-OUTPUT PARAMETER
  - list: linked list of data structs

This function saves a new data struct to the end of 'list'.
*/
void save_data(struct data *new, struct data **list){
  struct data *temp = *list;

  if(temp == NULL){
    *list = new;
  }
  else{

    while(temp->next != NULL){
      temp = temp->next;
    }

    temp->next = new;
  }

}

/*
INPUT PARAMETER
  - list: linked list of data structs

This function returns the number of nodes (data structs) in 'list'.
*/
int storage_status(struct data *list){
  int count = 0;
  struct data *temp = list;

  while(temp != NULL){
    count++;
    temp = temp->next;
  }

  return count;
}

/*
INPUT PARAMETER
  - mip_addr: MIP address

INPUT-OUTPUT PARAMETER
  - list: linked list of data structs

This function removes the first data struct with a MIP destination address
equal 'mip_addr'. 
*/
void remove_data(uint8_t mip_addr, struct data **list){
  struct data *temp = *list;
  struct data *prev = NULL;

  while(temp != NULL){
    // found node!
    if(temp->dst == mip_addr){
      // first node of list?
      if(prev == NULL){
        // first and only node of list?
        if(temp->next == NULL){
          *list = NULL;
          free(temp);
          break;
        }

        *list = temp->next;
        free(temp);
        break;
      }
      // last node in list?
      else if(temp->next == NULL){
        prev->next = NULL;
        free(temp);
        break;
      }
      // middle node of list?
      else{
        prev->next = temp->next;
        free(temp);
        break;
      }

    }

    prev = temp;
    temp = temp->next;
  }

  if(*list == NULL)
    fprintf(stderr, "Empty list!\n");

}

/*
INPUT PARAMETER
  - data_size: size of data

This function checks to see if 'data_size' meets the requirements for sending.
0 is returned if it does, else -1 is returned. 
*/
int size_check(int data_size){
  // data_size not zero?
  if(data_size > 0){
    // data_size a multiple of 4?
    if(data_size % 4 == 0){
      // data_size less than or equal to 1500 bytes?
      if(data_size + MIP_HDR_SIZE <= 1500){

        return 0;
      }

      fprintf(stderr, "SIZE OF DATAGRAM EXCEEDS 1500 BYTES\n");
      fprintf(stderr, "SIZE OF DATAGRAM IS THROWN\n");

      return -1;
    }

    fprintf(stderr, "SIZE OF DATAGRAM IS NOT A MULTIPLE OF 4\n");
    fprintf(stderr, "SIZE OF DATAGRAM IS THROWN\n");
  }

  return -1;
}

/*
INPUT PARAMETERS
  - mip_addr: MIP address
  - list: linked list of data structs

This function returns the first data struct with MIP destination address equal
to 'mip_addr' in list. If such a struct is not found, NULL is returned.
*/
struct data *get_data(uint8_t mip_addr, struct data *list){
  struct data *temp = list;

  while(temp != NULL){

    if(temp->dst == mip_addr){
      return temp;
    }

    temp = temp->next;
  }

  return NULL;
}

/*
INPUT PARAMETERS
  - dst: MAC destination address
  - src: MAC source address
  - packet: MIP header + data
  - packet: size of packet

INPUT-OUTPUT PARAMETER
  - eth_frame: frame struct

This function initialises 'eth_frame'.
*/
void init_frame(struct frame *eth_frame, uint8_t *dst, uint8_t *src, \
                                                char *packet, int packet_size){
  memcpy(eth_frame->dst, dst, MAC_SIZE);
  memcpy(eth_frame->src, src, MAC_SIZE);
  eth_frame->protocol = htons(ETH_P_MIP);
  memcpy(eth_frame->data, packet, packet_size);
}

/*
INPUT
*/
int send_frame(struct interface *ifa, char *packet, int packet_size){
  int retv;
  int frame_size = sizeof(struct frame) + packet_size;
  struct frame *eth_frame = malloc(frame_size);
  
  init_frame(eth_frame, ifa->mac_dst, ifa->mac_src, packet, packet_size);

  retv = send(ifa->sockfd, eth_frame, frame_size, 0);
  if(retv == -1){
    perror("send_frame(): send()");
    free(eth_frame);
    return -1;
  }

  free(eth_frame);

  return 0;
}

int broadcast(struct interface *my_interfaces, uint8_t mip_addr){
  int retv;
  char *mip_hdr;

  struct interface *temp = my_interfaces;
  while(temp != NULL){
    mip_hdr = create_miphdr(1, mip_addr, temp->mip_src, 0, 15);

    if(debug)
      print_status(temp->mac_dst, temp->mac_src, mip_addr, temp->mip_src);

    retv = send_frame(temp, mip_hdr, MIP_HDR_SIZE);
    if(retv == -1){
      free(mip_hdr);
      return -1;
    }

    free(mip_hdr);
    temp = temp->next;
  }

  return 0;
}

/*
Input parameter:
  - sockfd: socket

This function recv a frame from sockfd, stores it in a buffer and returns the 
buffer.
*/
struct frame *recv_frame(int sockfd){
  int retv;
  struct frame *eth_frame;
  int frame_size = BUF_SIZE + sizeof(struct frame);
  char frame_buf[frame_size];

  memset(frame_buf, 0, frame_size);

  retv = recv(sockfd, frame_buf, frame_size, 0);
  if(retv == 0){
    DLOG("connection closed!\n");
    return NULL;
  }
  else if(retv == -1){
    perror("recv_frame(): recv()");
    return NULL;
  }

  eth_frame = malloc(retv);
  memcpy(eth_frame, frame_buf, retv);

  return eth_frame;
}

int send_segment(int sockfd, uint8_t mip_addr, char *seg, int seg_size){
  int retv;

  struct iovec iov[2];
  iov[0].iov_base = &mip_addr;
  iov[0].iov_len = sizeof(uint8_t);
  iov[1].iov_base = seg;
  iov[1].iov_len = seg_size;

  struct msghdr msg = { 0 };
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;

  retv = sendmsg(sockfd, &msg, 0);
  if(retv == -1)
    perror("send_segment(): sendmsg()");

  return retv;
}

/*
INPUT PARAMETERS
  - tra: TRA-bits
  - mip_dst: MIP destination address of the msg to be sent
  - mip_src: MIP source address of the msg to be sent
  - ttl: Time-To-Live value

This function creates a MIP-header with the passed parameters and returns the
header.
*/
char *create_miphdr(uint8_t tra, uint8_t mip_dst, uint16_t mip_src, \
                                                  int data_size, uint8_t ttl){
  char *buf;
  uint8_t mip_hdr[MIP_HDR_SIZE] = { 0 };
  uint16_t payload = data_size;

  // not a broadcast or rsvp?
  if(data_size != 0){
    payload = MIP_HDR_SIZE + (data_size/4);
  }

  mip_hdr[0] = mip_hdr[0] | (tra << 5);
  mip_hdr[0] = mip_hdr[0] | (mip_dst >> 3);
  mip_hdr[1] = mip_hdr[1] | (mip_dst << 5);
  mip_hdr[1] = mip_hdr[1] | (mip_src >> 3);
  mip_hdr[2] = mip_hdr[2] | (mip_src << 5);
  mip_hdr[2] = mip_hdr[2] | (payload >> 4);
  mip_hdr[3] = mip_hdr[3] | (payload << 4);
  mip_hdr[3] = mip_hdr[3] | ttl;


  buf = malloc(MIP_HDR_SIZE);
  memcpy(buf, mip_hdr, MIP_HDR_SIZE);

  return buf;
}

/*
INPUT PARAMETERS
  - mip_hdr: MIP header
  - hdr_size: size of MIP header
  - msg: message to be sent
  - msg_size: size of message

OUTPUT PARAMETER
  - buf_ptr: pointer to the new appended array

This function creates a new array 'buf' where 'msg' is appended to the end of 
'mip_hdr'. 'buf' is then allocated in 'buf_ptr', and 'buf_ptr' is returned.  
*/
char *add_miphdr(char *mip_hdr, int hdr_size, char *msg, int msg_size){
  char mip_buf[hdr_size];
  char msg_buf[msg_size];

  int buf_size = hdr_size + msg_size;
  char buf[buf_size];
  char *buf_ptr;

  memcpy(mip_buf, mip_hdr, hdr_size);
  memcpy(msg_buf, msg, msg_size);

  int i;
  int j = 0;
  for(i=0; i<buf_size; i++){
    if(i >= hdr_size){
      buf[i] = msg_buf[j];
      j++;
    }
    else{
      buf[i] = mip_buf[i];
    }
  }

  buf_ptr = malloc(buf_size);
  memcpy(buf_ptr, buf, buf_size);

  return buf_ptr;
}

/*
INPUT PARAMETER
  - data: MIP header and a message from a received frame struct

This function extract and decrypt a MIP header som 'data' and stores the 
information in a header struct. The header struct is then returned.
*/
struct header *get_header(char *data){
  struct header *mip_hdr = malloc(sizeof(struct header));
  uint8_t buf[MIP_HDR_SIZE] = { 0 };
  memcpy(buf, data, MIP_HDR_SIZE);

  uint8_t tra = buf[0] >> 5;
  uint8_t dst = (buf[0] << 3) | (buf[1] >> 5);
  uint8_t src = (buf[1] << 3) | (buf[2] >> 5);
  uint16_t payload = buf[2];
  payload = payload & 31; //0001 1111
  payload = payload << 4;
  payload = payload | (buf[3] >> 4);
  uint8_t ttl = buf[3] & 15;

  mip_hdr->tra = tra;
  mip_hdr->dst = dst;
  mip_hdr->src = src;
  mip_hdr->payload = payload;
  mip_hdr->ttl = ttl;

  return mip_hdr;
}

/*
INPUT PARAMETER
  - sockfd: socket file descpriptor where the update is received from

OUTPUT PARAMETER
  - update: DVR table update

This function receives a DVR table update from 'sockfd' and returns the update.
If an error occur, NULL is returned.
*/
char *recv_update(int sockfd, int *bytes){
  ssize_t retv;
  char *update;

  char buf[BUF_SIZE] = { 0 };

  retv = recv(sockfd, buf, BUF_SIZE, 0);
  if(retv == 0){
    fprintf(stderr, "connection closed!\n");
    return NULL;
  }
  else if(retv == -1){
    perror("recv_update(): recv()");
    return NULL;
  }

  *bytes = retv;

  update = malloc(retv);
  memcpy(update, buf, retv);

  return update;
}

/**/
int request_route(int sockfd, uint8_t mip_addr){
  int retv;

  retv = send(sockfd, &mip_addr, sizeof(uint8_t), 0);
  if(retv == -1){
    perror("request_route(): send()");
    return -1;
  }

  return 0;
}

/**/
int recv_route(int sockfd, uint16_t *route){
  int retv;
  uint16_t buf = 0;

  retv = recv(sockfd, &buf, sizeof(uint16_t), 0);
  if(retv == -1){
    perror("recv_route(): recv()");
  }
  else if(retv == 0){
    fprintf(stderr, "connection closed!\n");
  }

  *route = buf;

  return retv;
}

char *create_update(struct interface *list, int update_size){
  int count = 0;
  uint8_t update[update_size];

  struct interface *temp = list;

  while(temp != NULL){
    if(count < update_size){
      update[count++] = temp->mip_dst;
    }

    temp = temp->next;
  }

  char *update_ptr = malloc(update_size);
  memcpy(update_ptr, update, update_size);

  return update_ptr;
}