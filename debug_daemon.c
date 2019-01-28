#include "daemon.h"

/*
INPUT PARAMETER
  - ifnames: linked list of allocated ifname structs

This function prints out the name in each node of linked list ifnames.
Used for debugging purposes.
*/
void print_names(struct ifname *ifnames){
  struct ifname *temp = ifnames;

  fprintf(stderr, "INTERFACES:\n");
  while(temp != NULL){
    fprintf(stderr, "%s\n", temp->name);
    temp = temp->next;
  }
}

/*
Input parameter:
  - mac[6]: mac address

This function prints out the mac address.
Used for debugging purposes.
*/
void print_mac(uint8_t mac[6]){
  int i;
  for(i=0; i<5; i++){
    fprintf(stdout, "%x.", mac[i]);
  }
  fprintf(stdout, "%x\n", mac[5]);
}

/*
Input parameter:
  - ifa: interface

This function prints the stored data on ifa.
Used for debugging purposes.
*/
void print_interface(struct interface *ifa){
  if(ifa != NULL){
    fprintf(stderr, "FILE DESCRIPTOR: %d\n", ifa->sockfd);
    fprintf(stderr, "MIP DST ADDRESS: %d\n", ifa->mip_dst);
    fprintf(stderr, "MIP SRC ADDRESS: %d\n", ifa->mip_src);
    fprintf(stderr, "MAC DST ADDRESS: ");
    print_mac(ifa->mac_dst);
    fprintf(stderr, "MAC SRC ADDRESS: ");
    print_mac(ifa->mac_src);
  }
  else{
    fprintf(stderr, "interface is NULL\n");
  }
}

/*This function prints out the global linked list arp_cache*/
void print_list(struct interface *list){
  struct interface *temp = list;

  fprintf(stderr, "ARP cache status:\n");
  if(temp != NULL){
    while(temp != NULL){
      fprintf(stderr, "FILE DESCRIPTOR: %d\n", temp->sockfd);
      fprintf(stderr, "MIP DST ADDRESS: %d\n", temp->mip_dst);
      fprintf(stderr, "MIP SRC ADDRESS: %d\n", temp->mip_src);
      fprintf(stderr, "MAC DST ADDRESS: ");
      print_mac(temp->mac_dst);
      fprintf(stderr, "MAC SRC ADDRESS: ");
      print_mac(temp->mac_src);
      temp = temp->next;
    }
    fprintf(stderr, "\n");
    return;
  }
  fprintf(stderr, "NOTHING IN LIST!\n\n");
}

void print_data(struct data *list){
  int i;
  uint8_t seqnum;
  char line[61] = { 0 };
  struct data *temp = list;

  for(i=0; i<60; i++){
  	line[i] = '-';
  }

  fprintf(stderr, "\n%28s\n", "DATA");
  fprintf(stderr, "%s\n", line);

  while(temp != NULL){
  	fprintf(stderr, "MIP destination: %d\n", temp->dst);
  	fprintf(stderr, "MIP source: %d\n", temp->src);
  	fprintf(stderr, "Time-to-live: %d\n", temp->ttl);
  	fprintf(stderr, "Data size: %d\n", temp->data_size);

  	memcpy(&seqnum, &temp->datagram[3], sizeof(uint8_t));
  	fprintf(stderr, "Sequence number: %d\n", seqnum);
  	// fprintf(stderr, "%s\n", &temp->datagram[4]);

  	fprintf(stderr, "%s\n", line);
    temp = temp->next;
  }

}

/*
INPUT PARAMETER
  - miphdr: MIP header

This function prints 'miphdr' to the terminal.
Used for debugging purposes.
*/
void print_miphdr(char *miphdr){
  unsigned char buf[4];
  memcpy(buf, miphdr, sizeof(buf));
  int len = sizeof(buf);
  int i;
  for(i=0; i<len; i++){
    fprintf(stderr, "[%d]: %d\n", i, buf[i]);
  }
}

/*
INPUT PARAMETER
  - mip_hdr: MIP header to be printed

This function prints out a struct header to the terminal. 
Used for debugging purposes.
*/
void print_header(struct header *mip_hdr){
  fprintf(stderr, "MIP header: \n");
  fprintf(stderr, "TRA: %d\n", mip_hdr->tra);
  fprintf(stderr, "Destination: %d\n", mip_hdr->dst);
  fprintf(stderr, "Source: %d\n", mip_hdr->src);
  fprintf(stderr, "Payload: %d\n", mip_hdr->payload);
  fprintf(stderr, "TTL: %d\n", mip_hdr->ttl);
}

/*this function prints out the frame provided*/
void dump_frame(struct frame *frame){
  fprintf(stderr, "\n---- FRAME ----\n");
  fprintf(stderr, "SOURCE MAC ADDRESS: ");
  print_mac(frame->src);
  fprintf(stderr, "DESTINATION MAC ADDRESS: ");
  print_mac(frame->dst);
  fprintf(stderr, "PROTOCOL: %d\n", frame->protocol);
  fprintf(stderr, "MESSAGE: %s\n\n", frame->data);
}

/*
INPUT PARAMETERS
  - update: update to be read
  - update_len: length of 'update'

This function prints 'update' to terminal.
Used for debugging purposes.
*/
void print_update(char *update, int update_size){
  uint8_t buf[update_size];
  memcpy(buf, update, update_size);

  int count = 0;
  fprintf(stderr, "-- UPDATE --\n");
  while(count < update_size){
    if(count == 0){
      fprintf(stderr, "%d\n", buf[count++]);
    }
    // end of update?
    else if(buf[count] == 255){
      fprintf(stderr, "%d\n", buf[count]);
      break;
    }

    fprintf(stderr, "%d  ", buf[count++]);
    fprintf(stderr, "%d\n", buf[count++]);
  } 
}

void print_status(uint8_t mac_dst[6], uint8_t mac_src[6], uint8_t mip_dst, \
                                                              uint8_t mip_src){
  fprintf(stderr, "Destination MAC-address: ");
  print_mac(mac_dst);
  fprintf(stderr, "Source MAC-address: ");
  print_mac(mac_src);
  fprintf(stderr, "Destination MIP-address: %d\n", mip_dst);
  fprintf(stderr, "Source MIP-address: %d\n\n", mip_src);
}