#include "debug.h"
#include "tp.h"
#include "sock.h"

int debug;
int epoll_fd;
fdcontext_t *fd_list[FDMAX];

int main(int argc, char *argv[]){
	int retv, timeout, running, i, count, fd;
	int app_listen, mipfd;
	fdcontext_t* fdctx;
	char *mip_path, *app_path;
	struct epoll_event events[15]; // increase if things starts fucking up

	memset(fd_list, 0, sizeof(fd_list));
	memset(events, 0, sizeof(events));

	retv = handle_argv(argc, argv);
	if(retv == -1)
		exit(EXIT_SUCCESS);

	mip_path = argv[optind];
	app_path = argv[optind+1];
	timeout = strtol(argv[optind+2], NULL, 10);

	// unused variable warning
	fprintf(stderr, "TIMEOUT: %d\n", timeout);

	epoll_fd = epoll_create(FDMAX);

	DLOG("creating listening socket");
  app_listen = create_listenfd(app_path);
  if(app_listen == -1){
  	exit(EXIT_FAILURE);
  }

  fdctx = malloc(sizeof(fdcontext_t));
  init_fdctx(fdctx, app_listen, 0, 0, NULL, NULL, 0);

  retv = add_fd(fdctx, epoll_fd, fd_list, FDMAX);
	if(retv == -1){
		close(app_listen);
		cleanup_fdctx(fdctx);
		close(epoll_fd);
		return EXIT_FAILURE;
	}

	DLOG("creating MIP deamon socket");
  mipfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if(mipfd == -1){
    perror("main(): socket()");
    cleanup_list(fd_list, FDMAX);
    close(epoll_fd);
    return(EXIT_FAILURE);
  }

	DLOG("connecting to MIP daemon");
	retv = connect_socket(mipfd, mip_path);
	if(retv == -1){
		close(mipfd);
		cleanup_list(fd_list, FDMAX);
		close(epoll_fd);
		return EXIT_FAILURE;
	}

  fdctx = malloc(sizeof(fdcontext_t));
  init_fdctx(fdctx, mipfd, 0, 0, NULL, NULL, 0);

	retv = add_fd(fdctx, epoll_fd, fd_list, FDMAX);
	if(retv == -1){
		close(mipfd);
		cleanup_fdctx(fdctx);
		cleanup_list(fd_list, FDMAX);
		close(epoll_fd);
		return EXIT_FAILURE;
	}

	running = 1;
	while(running){
		DLOG("polling for activity...");
		count = epoll_wait(epoll_fd, events, 15, -1);
		fprintf(stderr, "Number of ready events: %d\n", count);

		if(count == -1){
			running = 0;
		}

		for(i=0; i<count; i++){
			fdctx = events[i].data.ptr;
			fd = fdctx->fd;

			if(fd == app_listen){
				DLOG("New application!");
				int newfd;

				newfd = init_connection(app_listen, app_path);
				if(newfd == -1){
					running = 0;
				}
				else{
				  fdcontext_t *newctx = malloc(sizeof(fdcontext_t));
				  init_fdctx(newctx, newfd, 0, 0, NULL, NULL, 0);

					retv = add_fd(newctx, epoll_fd, fd_list, FDMAX);
					if(retv == -1){
						fprintf(stderr, "Cannot add new fd applications!\n");
						close(newfd);
						cleanup_fdctx(newctx);
						running = 0;
					}

				}

			}
			else if(fd == mipfd){
				int seg_size;
				uint8_t mip_src, pl;
				char *segment = malloc(FRAG_SIZE + TP_SIZE);

				DLOG("receiving segment from MIP daemon");
				retv = recv_segment(fd, &mip_src, segment);
				if(retv <= 0){
					free(segment);
					running = 0;
				}

				if(retv > 0){
					memcpy(&pl, segment, sizeof(pl));
					pl = pl >> 6;
					seg_size = retv - sizeof(mip_src);

					// ack?
					if(seg_size == TP_SIZE && pl == 1){
						DLOG("ack received!");
						int index;
						header_t *hdr = malloc(sizeof(header_t));
						init_header(segment, hdr);
						
						if(debug){
							fprintf(stderr, "\n");
							print_hdr(hdr);
							fprintf(stderr, "\n");
						}

						index = find_port(hdr->port, fd_list, FDMAX);
						if(index != -1){
							if(in_window(hdr, fd_list[index]->s_win)){

								DLOG("updating window");
								retv = update_sender(mipfd, fd_list[index], timeout);
								if(retv == -1){
									running = 0;
								}
								else if(retv == 1){
									// file completely sent
									cleanup_fdctx(fd_list[index]);
								}

							}

						}

						free(hdr);
					}
					// fragment?
					else if(seg_size > TP_SIZE){
						header_t *hdr = malloc(sizeof(header_t));
						init_header(segment, hdr);
						
						if(debug)
							print_hdr(hdr);

						retv = receiver_check(mip_src, hdr);
						if(retv == -1){
							fprintf(stderr, "No applications listening on port!\n");
						}
						else if(!retv){
							char *ack;
							int index = find_port(hdr->port, fd_list, FDMAX);
							fdcontext_t *temp = fd_list[index];

							// pl == 1
							ack = create_tphdr(TP_SIZE+3, hdr->port, hdr->seqnum);

							DLOG("resending ack");
							retv = send_segment(mipfd, temp->mip_addr, ack, TP_SIZE);
							if(retv == -1)
								running = 0;

							free(ack);
						}
						else{
							char *ack;
							int index = find_port(hdr->port, fd_list, FDMAX);
							fdcontext_t *temp = fd_list[index];

							DLOG("saving fragment");
							save_fragment(temp->r_win, hdr, segment, seg_size);

							DLOG("updating receiver");
							retv = update_receiver(temp->r_win, hdr->seqnum);
							if(retv){
								fprintf(stderr, "Received all fragments!\n");
								// send fragments to server
							}

							// pl == 1
							ack = create_tphdr(TP_SIZE+3, hdr->port, hdr->seqnum);

							DLOG("sending ack");
							retv = send_segment(mipfd, temp->mip_addr, ack, TP_SIZE);
							if(retv == -1)
								running = 0;

							free(ack);
						}

						free(hdr);
					}

				}

				free(segment);
			}
			// Timeout event!
			else if(fdctx->timer == 1){

				DLOG("timeout event!");
				retv = timeout_event(mipfd, fdctx, timeout);
				if(retv == -1){
					running = 0;
				}
				else if(!retv){
					uint16_t port = fdctx->port;
					int index = find_port(port, fd_list, FDMAX);

					while(index != -1){
						cleanup_fdctx(fd_list[index]);
						index = find_port(port, fd_list, FDMAX);
					}

				}

			}
			// application event!
			else{
				uint8_t mip_addr;
				uint16_t port, filesize;

				DLOG("receiving fileinfo");
				retv = recv_fileinfo(fd, &mip_addr, &port, &filesize);
				if(retv == -1){
					running = 0;
				}
				if(retv == 0){
					close(fdctx->fd);
				}

				if(retv > 0){
					init_fdctx(fdctx, fdctx->fd, port, mip_addr, NULL, NULL, 0);

					// new server?
					if(!mip_addr){
						DLOG("initialising new server");
						new_server(fdctx);
					}
					// new client?
					else{
						fprintf(stderr, "MIP ADDR: %d\n", mip_addr);

						DLOG("initialising new client");
						retv = new_client(fdctx, filesize);
						if(retv == -1){
							running = 0;

						// leak in new_client, lost pointer to sender window??
						}
						else{

							DLOG("sending window");
							retv = send_window(mipfd, fdctx, timeout);
							if(retv == -1)
								running = 0;

						}
						
					}
					
				}

				// sender properly added to fd_list?
				int test_index = get_seat(fdctx->fd, fd_list, FDMAX);
				if(test_index != -1){
					fprintf(stderr, "test_index: %d\n", test_index);
				}
				// end of test

			}

		}

	}

	cleanup_list(fd_list, FDMAX);
	close(epoll_fd);

	return EXIT_SUCCESS;
}