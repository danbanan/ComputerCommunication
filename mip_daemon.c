#include "sock.h"
#include "daemon.h"
#include "debug.h"

int debug;

int main (int argc, char *argv[]){
  int retv, ifcount, num_of_mips, count;
  int tp_listen, fwd_listen, rt_listen;
  char *tp_path, *fwd_path, *rt_path;

  int fdmax = 0;
  int tp_fd = 0;
  int fwd_fd = 0;
  int rt_fd = 0;

  struct interface *my_interfaces = NULL;
  struct interface *arp_cache = NULL;
  struct data *data_list = NULL;
  fd_set master, readfds, writefds;

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&master);

  retv = handle_args(argc, argv);
  if(retv == -1)
    exit(EXIT_SUCCESS);

  tp_path = argv[optind];
  fwd_path = argv[optind+1];
  rt_path = argv[optind+2];

/* ------------------------------------------------------------------------- */
  ifcount = 0;
  num_of_mips = argc-3-optind; //3 sockpaths in cmd-line

  struct ifaddrs *interface_list = get_interface_names();
  struct ifname *ifnames = get_ethernet_names(interface_list, &ifcount);
  freeifaddrs(interface_list);

  if(num_of_mips != ifcount){
    fprintf(stderr, "NUMBER OF MIP ADDRESSES EXPECTED: %d\n", ifcount);
    exit(EXIT_SUCCESS);
  }

/* ------------------------------------------------------------------------- */
  DLOG("Initializing a raw socket on each local interface...");
  /*
  This loop creates new interface struct and stores them in my_interfaces
  */
  // init_raw_sockets() ??
  count = optind+3; //index of the first MIP address in argv
  struct ifname *temp = ifnames;
  while(temp != NULL){
    // reading within argv[]
    if(count < argc){
      uint8_t mac[6] = { 0 };
      uint8_t mac_broadcast[6] = {255, 255, 255, 255, 255, 255};
      uint8_t mip_addr = strtol(argv[count], NULL, 10);
      int rawfd = init_rawfd(temp->name); //feil i valgrind bind()
      get_mac_addr(rawfd, mac, temp->name);

      struct interface *new = malloc(sizeof(struct interface));

      init_interface(new, rawfd, mip_addr, mip_addr, mac_broadcast, mac);
      add_interface(new, &my_interfaces);

      FD_SET(rawfd, &master);
      update_fdmax(rawfd, &fdmax);
    }

    temp = temp->next;
    count++;
  }

  free_names(ifnames);

  if(debug){
    fprintf(stderr, "\n-- LOCAL INTERFACE(S)--\n");
    print_list(my_interfaces);
  }

/* ------------------------------------------------------------------------- */
  DLOG("creating listening sockets");

  tp_listen = create_listenfd(tp_path);
  if(tp_listen == -1){
    close_all(master, fdmax);
    free_interfaces(my_interfaces);
    exit(EXIT_FAILURE);
  }

  FD_SET(tp_listen, &master);
  update_fdmax(tp_listen, &fdmax);

  fwd_listen = create_listenfd(fwd_path);
  if(fwd_listen == -1){
    close_all(master, fdmax);
    free_interfaces(my_interfaces);
    exit(EXIT_FAILURE);
  }

  FD_SET(fwd_listen, &master);
  update_fdmax(fwd_listen, &fdmax);

  rt_listen = create_listenfd(rt_path);
  if(rt_listen == -1){
    close_all(master, fdmax);
    free_interfaces(my_interfaces);
    exit(EXIT_FAILURE);
  }

  FD_SET(rt_listen, &master);
  update_fdmax(rt_listen, &fdmax);

/* ------------------------------------------------------------------------- */

  for(;;){
    DLOG("looking for activiy in socket set...");
    readfds = master; //a copy set to keep track of all connections
    retv = select(fdmax+1, &readfds, NULL, NULL, NULL);
    if(retv == -1){
      perror("main(): select()");
      clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
      exit(EXIT_FAILURE);
    }
    DLOG("found activity!\n");

    int i;
    for(i=0; i<=fdmax; i++){
      // in fd_set?
      if(FD_ISSET(i, &readfds)){

        if(i == tp_listen){
          DLOG("connecting transport daemon...");
          tp_fd = init_connection(i, tp_path);
          if(tp_fd == -1){
            clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
            exit(EXIT_FAILURE);
          }

          FD_SET(tp_fd, &master);
          FD_SET(tp_fd, &writefds);
          update_fdmax(tp_fd, &fdmax);
        }
        else if(i == fwd_listen){
          DLOG("connecting forwarding socket...");
          fwd_fd = init_connection(i, fwd_path);
          if(fwd_fd == -1){
            clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
            exit(EXIT_FAILURE);
          }

          FD_SET(fwd_fd, &master);
          update_fdmax(fwd_fd, &fdmax);
        }
        else if(i == rt_listen){
          char *update;

          DLOG("connecting routing socket...");
          rt_fd = init_connection(i, rt_path);
          if(rt_fd == -1){
            clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
            exit(EXIT_FAILURE);
          }

          FD_SET(rt_fd, &master);
          update_fdmax(rt_fd, &fdmax);

          update = create_update(my_interfaces, num_of_mips);
          retv = send(rt_fd, update, num_of_mips, 0);
          if(retv == -1){
            perror("main(): send()");
            free(update);
            clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
            exit(EXIT_FAILURE);
          }

          free(update);
        }
        else if(i == tp_fd){
          uint8_t mip_addr;
          int data_size = 0;
          char *data_buf = malloc(BUF_SIZE);
          memset(data_buf, 0, BUF_SIZE); 

          DLOG("receiving message from application");
          retv = recv_data(i, &mip_addr, data_buf);
          // MIP daemon does not shutdown, because it can still be useful as a
          // router even if communication with the application is cut out.
          if(retv <= 0){
            FD_CLR(i, &master);
            close(i);
          }

          fprintf(stderr, "Number of bytes received: %d\n", retv);

          data_size = retv - sizeof(mip_addr);

          if(size_check(data_size) != -1){
            struct data *new;

            DLOG("requesting route from router");
            retv = request_route(fwd_fd, mip_addr);
            if(retv == -1){
              free(data_buf);
              clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
              exit(EXIT_FAILURE);
            }

            // adding null-byte - invalid pointer when debug-printing
            new = malloc(sizeof(struct data) + data_size + 1);
            memset(new, 0, sizeof(struct data) + data_size + 1);

            init_data(new, mip_addr, 0, 15, data_size, data_buf);
            save_data(new, &data_list);

            if(storage_status(data_list) > 100){
              // remove first node of the list
              remove_data(data_list->dst, &data_list);
            }

            if(debug)
              print_data(data_list);

          }

          free(data_buf);
        }
        else if(i == fwd_fd){
          uint8_t mip_end, mip_next;
          uint16_t route;

          DLOG("receiving route from router");
          retv = recv_route(i, &route);
          if(retv <= 0){
            clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
            exit(EXIT_FAILURE);
          }

          mip_end = route >> 8;
          mip_next = route;

          if(mip_next){
            struct interface *temp = get_interface(arp_cache, mip_next);

            if(temp != NULL){
              struct data *dgram = get_data(mip_end, data_list);

              // message still in data_list?
              if(dgram != NULL){
                char *mip_hdr, *packet;
                uint16_t packet_size;

                // missing source address?
                if(dgram->src == 0){
                  dgram->src = temp->mip_src;
                }

                mip_hdr = create_miphdr(4, dgram->dst, dgram->src, \
                                              dgram->data_size, dgram->ttl - 1);
                packet = add_miphdr(mip_hdr, MIP_HDR_SIZE, dgram->datagram, \
                                                            dgram->data_size);
                packet_size = MIP_HDR_SIZE + dgram->data_size;

                remove_data(dgram->dst, &data_list);

                DLOG("forwarding datagram");
                if(debug)
                  print_status(temp->mac_dst, temp->mac_src, dgram->dst, \
                                                                  dgram->src);

                retv = send_frame(temp, packet, packet_size);
                if(retv == -1){
                  free(mip_hdr);
                  free(packet);
                  clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
                  exit(EXIT_FAILURE);
                }

                free(mip_hdr);
                free(packet);
              }
              else{
                fprintf(stderr, "Datagram not found!\n");
              }

            }
            else{

              DLOG("broadcasting");
              retv = broadcast(my_interfaces, mip_next);
              if(retv == -1){
                clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
                exit(EXIT_FAILURE); 
              }

            }

          }
          else{
            fprintf(stderr, "Route to destination (%d) is UNAVAILABLE!\n", mip_end);
          }
          
          if(debug){
            fprintf(stderr, "ARP cache status:\n");
            print_list(arp_cache);
          }

        }
        else if(i == rt_fd){
          char *update, *mip_hdr, *packet;
          int update_size, packet_size;
          struct interface *temp;
          
          DLOG("receiving routing update from router");
          update = recv_update(i, &update_size);
          if(update == NULL){
            clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
            exit(EXIT_FAILURE);
          }

          // no neighbors in table?
          if((uint8_t)update[0] == 255){
            temp = my_interfaces;

            while(temp != NULL){
              // first byte of update is the source address
              update[0] = temp->mip_src;
              mip_hdr = create_miphdr(2, 255, temp->mip_src, update_size, 0);
              packet = add_miphdr(mip_hdr, MIP_HDR_SIZE, update, update_size);
              packet_size = MIP_HDR_SIZE + update_size;

              DLOG("broadcasting DVR-table update");
              if(debug)
                print_status(temp->mac_dst, temp->mac_src, 255, temp->mip_src);

              retv = send_frame(temp, packet, packet_size);
              if(retv == -1){
                free(update);
                free(mip_hdr);
                free(packet);
                clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
                exit(EXIT_FAILURE);
              }

              free(mip_hdr);
              free(packet);
              temp = temp->next;
            }

          }
          else{
            temp = get_interface(arp_cache, (uint8_t)update[0]);

            if(temp != NULL){
              // first byte of update is the source address
              update[0] = temp->mip_src;
              mip_hdr = create_miphdr(2, 255, temp->mip_src, update_size, 0);
              packet = add_miphdr(mip_hdr, MIP_HDR_SIZE, update, update_size);
              packet_size = MIP_HDR_SIZE + update_size;

              DLOG("broadcasting DVR-table update");
              if(debug)
                print_status(temp->mac_dst, temp->mac_src, 255, temp->mip_src);

              retv = send_frame(temp, packet, packet_size);
              if(retv == -1){
                free(update);
                free(mip_hdr);
                free(packet);
                clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
                exit(EXIT_FAILURE);
              }

              free(mip_hdr);
              free(packet);
            }
            else{
              DLOG("broadcasting arp-request");
              retv = broadcast(my_interfaces, (uint8_t)update[0]);
              if(retv == -1){
                clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
                exit(EXIT_FAILURE); 
              }

            }

          }

          free(update);
        }
        else{ // message from neighbor mip
          struct header *mip_hdr;
          struct frame *eth_frame;
          struct interface *temp;

          DLOG("receiving frame from neighbor daemon");
          eth_frame = recv_frame(i);
          if(eth_frame == NULL){
            clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
            exit(EXIT_FAILURE); 
          }

          mip_hdr = get_header(eth_frame->data);

          if(debug)
            print_status(eth_frame->dst, eth_frame->src, mip_hdr->dst, \
                                                                mip_hdr->src);

          temp = get_interface(my_interfaces, mip_hdr->dst);
          // frame arrived to its destination?
          if(temp != NULL){
            // message to application?
            if(mip_hdr->tra == 4){
              int data_size = (mip_hdr->payload - MIP_HDR_SIZE) * 4;
              char *dgram = malloc(data_size);

              // copying past MIP header of frame->data to only copy the 
              // message
              memcpy(dgram, &eth_frame->data[MIP_HDR_SIZE], data_size);

              DLOG("sending segment to MIP-TP daemon");
              retv = send_segment(tp_fd, mip_hdr->src, dgram, data_size);
              // MIP daemon does not shutdown, because it can still be useful as a
              // router even if communication with TP daemon is down.
              if(retv == -1){
                FD_CLR(tp_fd, &master);
                close(tp_fd);
              }

              free(dgram);
            }
            // broadcast message?
            else if(mip_hdr->tra == 1){
              char *arp_hdr;
              struct interface *new = malloc(sizeof(struct interface));

              init_interface(new, i, mip_hdr->src, mip_hdr->dst, \
                                                eth_frame->src, temp->mac_src);
              add_interface(new, &arp_cache);

              arp_hdr = create_miphdr(0, new->mip_dst, new->mip_src, 0, 15);

              DLOG("sending arp-response");
              if(debug)
                print_status(new->mac_dst, new->mac_src, new->mip_dst, \
                                                                new->mip_src);

              retv = send_frame(new, arp_hdr, MIP_HDR_SIZE);
              if(retv == -1){
                free(arp_hdr);
                free(mip_hdr);
                free(eth_frame);
                clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
                exit(EXIT_FAILURE);
              }

              free(arp_hdr);
            }
            // arp-response?
            else if(mip_hdr->tra == 0){
              char *new_hdr, *packet;
              int packet_size;
              struct data *dgram;
              struct interface *new = malloc(sizeof(struct interface));

              init_interface(new, i, mip_hdr->src, mip_hdr->dst, \
                                              eth_frame->src, temp->mac_src);
              add_interface(new, &arp_cache);

              dgram = get_data(mip_hdr->src, data_list);
              while(dgram != NULL){
                // missing MIP source address?
                if(dgram->src == 0){
                  dgram->src = new->mip_src;
                }
                
                new_hdr = create_miphdr(4, dgram->dst, dgram->src, \
                                              dgram->data_size, dgram->ttl-1);

                packet = add_miphdr(new_hdr, MIP_HDR_SIZE, dgram->datagram, \
                                                            dgram->data_size);
                packet_size = MIP_HDR_SIZE + dgram->data_size;

                DLOG("forwarding datagram");
                retv = send_frame(new, packet, packet_size);
                if(retv == -1){
                  free(new_hdr);
                  free(packet);
                  free(mip_hdr);
                  free(eth_frame);
                  clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
                  exit(EXIT_FAILURE);
                }

                free(new_hdr);
                free(packet);
                remove_data(mip_hdr->src, &data_list);

                dgram = get_data(mip_hdr->src, data_list);
              }

            }

          }
          else{
            // DVR table update?
            if(mip_hdr->dst == 255){
              // copying past MIP header of frame->data to only copy the 
              // message
              int update_size = (mip_hdr->payload - MIP_HDR_SIZE) * 4;
              char *update = malloc(update_size);

              memcpy(update, &eth_frame->data[MIP_HDR_SIZE], update_size);

              DLOG("sending DVR-table update to router");
              retv = send(rt_fd, update, update_size, 0);
              if(retv == -1){
                perror("main(): send()");
                free(update);
              }

              free(update);
            }
            // datagram to be forwarded?
            else if(mip_hdr->tra == 4){
              char *dgram;
              int data_size;
              struct data *new;

              DLOG("requesting route from router");
              retv = request_route(fwd_fd, mip_hdr->dst);
              if(retv == -1){
                free(mip_hdr);
                free(eth_frame);
                clean_up(master, fdmax, data_list, arp_cache, my_interfaces);
                exit(EXIT_FAILURE);
              }

              data_size = (mip_hdr->payload - MIP_HDR_SIZE) * 4;
              dgram = malloc(data_size);

              memcpy(dgram, &eth_frame->data[MIP_HDR_SIZE], data_size);

              new = malloc(sizeof(struct data) + data_size + 1);
              memset(new, 0, sizeof(struct data) + data_size + 1);

              init_data(new, mip_hdr->dst, mip_hdr->src, mip_hdr->ttl, \
                                                            data_size, dgram);
              save_data(new, &data_list);
              
              if(storage_status(data_list) > 100)
                // remove first node of the list
                remove_data(data_list->dst, &data_list);

              if(debug)
                print_data(data_list);

              free(dgram);
            }

          }

          if(debug)
            print_list(arp_cache);

          free(mip_hdr);
          free(eth_frame);
        }

      }

    }

  }

  return 0;
}