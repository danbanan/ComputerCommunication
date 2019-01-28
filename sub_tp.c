#include "debug.h"
#include "sock.h"
#include "tp.h"

/*
INPUT PARAMETERS
  - arg_req: number of arguments requested to run the program
  - argc: number of arguments given when running the program
  - argv: arguments given when running the program

This function prints out how to run the program if the number of arguments 
given is not equal to the number of arguments required and 0 is returned.
*/
int proper_usage(int arg_req, int argc, char *argv[]){
	if(argc != arg_req){
		fprintf(stderr, "USAGE: %s [-d] <Socket_path> <Application_path> " \
																											"<Timeout>\n",	argv[0]);
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
int handle_argv(int argc, char *argv[]){
  opterr = 0; //to make getopt not print error message
  int retv = getopt(argc, argv, "d");
  if(retv == 'd'){

  	if(!proper_usage(5, argc, argv))
  		return -1;
  	
    debug = 1;
  }
  else if(!proper_usage(4, argc, argv)){
  	return -1;
  }

  return 0;
}

/*
INPUT PARAMETER
	- sockfd: socket file descriptor

INPUT-OUTPUT PARAMETERS
	- mip_addr: MIP address buffer
	- port: Port number buffer
	- filesize: size of file buffer

OUTPUT PARAMETER
	- retv: return value

This function receives information about a file (destination MIP address, 
destination Port number, size of the file) and stores in the passed buffers.
The return value of recvmsg() is returned.
*/
int recv_fileinfo(int sockfd, uint8_t *mip_addr, uint16_t *port, \
																													uint16_t *filesize){
	int retv;

  struct iovec iov[3];
  iov[0].iov_base = mip_addr;
  iov[0].iov_len = sizeof(uint8_t);
  iov[1].iov_base = port;
  iov[1].iov_len = sizeof(uint16_t);
  iov[2].iov_base = filesize;
  iov[2].iov_len = sizeof(uint16_t);

  struct msghdr msg = { 0 };
  msg.msg_iov = iov;
  msg.msg_iovlen = 3;

  retv = recvmsg(sockfd, &msg, 0);
  if(retv == -1){
  	perror("recv_fileinfo(): recvmsg()");
  }
  else if(!retv){
  	fprintf(stderr, "Connection closed!\n");
  }

	return retv;
}

/*
INPUT PARAMETERS
	- sockfd: socket file descriptor
	- filesize: size of file

OUTPUT PARAMETER
	- file: file received

This function receives a file from 'sockfd' and returns it. NULL is returned if
an error occur.
*/
char *recv_file(int sockfd, uint16_t filesize){
	int retv;
	char *file = malloc(filesize);

	retv = recv(sockfd, file, filesize, 0);
	if(retv == -1){
		perror("recv_file(): recv()");
		free(file);
		return NULL;
	}
	else if(!retv){
		fprintf(stderr, "Connection closed!\n");
		free(file);
		return NULL;
	}

	return file;
}

void free_fragments(sender_t *s_win, receiver_t *r_win){
	int i;

	if(s_win != NULL){
		sender_t *temp = s_win;

		for(i=0; i<temp->nof; i++){
			if(temp->fragments[i] != NULL){
				free(temp->fragments[i]);
			}
		}
	}

	if(r_win != NULL){
	 receiver_t *temp = r_win;

		for(i=0; i<temp->nof; i++){
			if(temp->fragments[i] != NULL){
				free(temp->fragments[i]);
			}
		}
	}
}

char *create_tphdr(uint16_t filesize, uint16_t port, uint8_t seqnum){
	uint8_t padding = 4 - (filesize % 4);
	uint8_t hdr[TP_SIZE] = { 0 };
	char *hdr_ptr;

	hdr[0] = padding << 6;
	hdr[0] = hdr[0] | (port >> 8);
	hdr[1] = port;
	hdr[3] = seqnum;

	hdr_ptr = malloc(TP_SIZE);
	memcpy(hdr_ptr, hdr, TP_SIZE);

	return hdr_ptr;
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
char *add_hdr(char *hdr, int hdr_size, char *file, int filesize){
  int i, j;
  char hdr_buf[hdr_size];
  char file_buf[filesize];

  int buf_size = hdr_size + filesize;
  char buf[buf_size];
  char *buf_ptr;

  memcpy(hdr_buf, hdr, hdr_size);
  memcpy(file_buf, file, filesize);

  j = 0;
  for(i=0; i<buf_size; i++){
    if(i >= hdr_size){
      buf[i] = file_buf[j];
      j++;
    }
    else{
      buf[i] = hdr_buf[i];
    }
  }

  buf_ptr = malloc(buf_size);
  memcpy(buf_ptr, buf, buf_size);

  return buf_ptr;
}

/*
INPUT PARAMETERS
  - sockfd: socket file descriptor connected to mip daemon on the same host
  - mip_addr: MIP address of the message destination
  - mip_msg: the message to be sent

This function initializes msghdr msg with mip_addr and mip_msg, and sends it
to the mip daemon on the same host through sockfd.
*/
int send_segment(int sockfd, uint8_t mip_addr, char *data, uint16_t data_size){
  int retv;
  int padding = data_size % 4;
  int bufsize = data_size;

  if(padding)
    bufsize = bufsize + 4 - padding;

  char buf[bufsize];
  memset(buf, 0, bufsize);
  memcpy(buf, data, data_size);

  struct iovec iov[2];
  iov[0].iov_base = &mip_addr;
  iov[0].iov_len = sizeof(mip_addr);
  iov[1].iov_base = buf;
  iov[1].iov_len = bufsize;

  struct msghdr msg = { 0 };
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;

  retv = sendmsg(sockfd, &msg, 0);
  if(retv == -1)
    perror("send_segment(): sendmsg()");

  return retv;
}

int recv_segment(int sockfd, uint8_t *mip_addr, char *buf){
	int retv;

	struct iovec iov[2];
  iov[0].iov_base = mip_addr;
  iov[0].iov_len = sizeof(uint8_t);
  iov[1].iov_base = buf;
  iov[1].iov_len = FRAG_SIZE + TP_SIZE;

  struct msghdr msg = { 0 };
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;

  retv = recvmsg(sockfd, &msg, 0);
  if(retv == -1)
  	perror("recv_segment(): recvmsg()");

  return retv;
}

// highest possible sequence number is 65535 / 1492 + 1(filesize) = 46.76 ~ 47
void init_header(char *seg, header_t *hdr){
	uint8_t buf[TP_SIZE];
	uint16_t temp;

	memcpy(buf, seg, TP_SIZE);

	hdr->pl = buf[0] >> 6;
	temp = buf[0] & 63; // 0011 1111
	temp = temp << 8;
	hdr->port = temp | buf[1];
	hdr->seqnum = buf[3];
}

int empty_seat(fdcontext_t *array[], int arrlen){
	int i;

	for(i=0; i<arrlen; i++){
		if(array[i] == NULL){
			return i;
		}
	}
	
	fprintf(stderr, "No empty seats in array!\n");
	return -1;	
}

int get_seat(int fd, fdcontext_t *array[], int arrlen){
	int i;
	fdcontext_t *temp;

	for(i=0; i<arrlen; i++){
		temp = array[i];

		if(temp != NULL){
			
			if(temp->fd == fd){
				return i;
			}

		}

	}
		
	fprintf(stderr, "file descpricptor not found in array!\n");
	return -1;	
}

void init_fdctx(fdcontext_t *fdctx, int fd, uint16_t port, uint8_t mip_addr, \
														sender_t *s_win, receiver_t *r_win, uint8_t timer){
	fdctx->fd = fd;
	fdctx->port = port;
	fdctx->mip_addr = mip_addr;
	fdctx->s_win = s_win;
	fdctx->r_win = r_win;
	fdctx->timer = timer;
}

void cleanup_fdctx(fdcontext_t *fdctx){
	int index = get_seat(fdctx->fd, fd_list, FDMAX);

	close(fdctx->fd);

	if(!fdctx->timer){
		free_fragments(fdctx->s_win, fdctx->r_win);
	}

	free(fdctx);
	fd_list[index] = NULL;
}


void cleanup_list(fdcontext_t *array[], int arrlen){
	int i;
	fdcontext_t *temp;

	for(i=0; i<arrlen; i++){
		temp = array[i];

		if(temp != NULL)
			cleanup_fdctx(temp);

	}

}

int add_fd(fdcontext_t *fdctx, int epollfd, fdcontext_t *array[], int arrlen){
	int retv, index;
	struct epoll_event event = { 0 };	

	event.events = EPOLLIN;
	event.data.ptr = fdctx;

	retv = epoll_ctl(epollfd, EPOLL_CTL_ADD, fdctx->fd, &event);
	if(retv == -1){
		perror("add_fd(): epoll_ctl()");
		return -1;
	}

	index = empty_seat(array, arrlen);
	if(index == -1)
		return -1;

	array[index] = fdctx;

	fprintf(stderr, "fd %d on index %d in fd_list\n", fdctx->fd, index);

	return index;
}

void new_server(fdcontext_t *fdctx){
	int i;
	receiver_t *win;

	win = malloc(sizeof(receiver_t));
	memset(win, 0, sizeof(receiver_t));

	for(i=0; i<45; i++){
		win->fragments[i] = NULL;
	}

	win->lfr = -1;
	win->laf = win->lfr + WIN_SIZE;
	fdctx->r_win = win;
}

int num_of_fragments(uint16_t filesize){
	int num;
	int rest = filesize % FRAG_SIZE;

	num = (filesize - rest) / FRAG_SIZE;
	num++; // first fragment is the filesize and a handshake

	if(rest != 0){
		num++; // last fragment is less than FRAG_SIZE
	}

	return num;
}

void fragment_file(sender_t *win, int nof, char *file, uint16_t filesize){
	fragment_t *new;
	int i;
	int offset = 0;
	int rest = filesize % FRAG_SIZE;

	for(i=0; i<nof; i++){
		// first fragment?
		if(i == 0){
			new = malloc(sizeof(fragment_t) + sizeof(uint16_t));

			new->ack = 0;
			new->timerfd = 0;
			new->data_size = sizeof(uint16_t);
			memcpy(new->data, &filesize, sizeof(uint16_t));
		}
		// last fragment and filesize not a multiple of FRAG_SIZE?
		else if(i == nof-1 && rest){
			new = malloc(sizeof(fragment_t) + rest);

			new->ack = 0;
			new->timerfd = 0;
			new->data_size = rest;
			memcpy(new->data, &file[offset], rest);
		}
		else{
			new = malloc(sizeof(fragment_t) + FRAG_SIZE);

			new->ack = 0;
			new->timerfd = 0;
			new->data_size = FRAG_SIZE;
			memcpy(new->data, &file[offset], FRAG_SIZE);

			offset += FRAG_SIZE;
		}

		win->fragments[i] = new;
	}

}

sender_t *create_sender(char *file, uint16_t filesize){
	sender_t *win;
	int	nof = num_of_fragments(filesize);
	int sendsize = sizeof(sender_t) + (sizeof(fragment_t *) * nof);

	win = malloc(sendsize);
	memset(win, 0, sendsize);

	win->nof = nof;
	fragment_file(win, nof, file, filesize);

	return win;
}

int new_client(fdcontext_t *fdctx, uint16_t filesize){
	sender_t *win;
	char *file;

	file = recv_file(fdctx->fd, filesize);
	if(file == NULL){
		return -1;
	}

	win = create_sender(file, filesize);
	fdctx->s_win = win;

	free(file);

	return 0;
}

int add_timer(fdcontext_t *fdctx){
	int retv, timerfd;

	timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if(timerfd == -1){
		perror("create_timer(): timerfd_create()");
		return -1;
	}

	fdctx->fd = timerfd;

	retv = add_fd(fdctx, epoll_fd, fd_list, FDMAX);

	return retv;
}

int start_timer(int timerfd, int timeout){
	int retv;
	struct itimerspec new_timer;

  new_timer.it_interval.tv_sec = timeout;
  new_timer.it_interval.tv_nsec = 0;
  new_timer.it_value.tv_sec = timeout;
  new_timer.it_value.tv_nsec = 0;

  retv = timerfd_settime(timerfd, 0, &new_timer, NULL);
  if(retv == -1)
  	perror("start_timer(): timerfd_settime()");

  return retv;
}


int send_window(int sockfd, fdcontext_t *fdctx, int timeout){
	int retv, i;
	char *hdr, *segment;
	uint16_t seg_size;
	sender_t *win = fdctx->s_win;
	fragment_t *temp;
	fdcontext_t *timer;

	for(i=0; i<WIN_SIZE; i++){
		// reading within array (less fragments than WIN_SIZE)
		if(i < win->nof){
			temp = win->fragments[i];

			hdr = create_tphdr(temp->data_size, fdctx->port, i);
			segment = add_hdr(hdr, TP_SIZE, temp->data, temp->data_size);
			seg_size = temp->data_size + TP_SIZE;

			timer = malloc(sizeof(fdcontext_t));
			init_fdctx(timer, 0, fdctx->port, fdctx->mip_addr, win, NULL, 1);

			retv = add_timer(timer);
			if(retv == -1){
				free(hdr);
				free(segment);
				return -1;
			}

			// adding to fragment
			temp->timerfd = timer->fd;
			temp->resent = 0;

			retv = start_timer(timer->fd, timeout);
			if(retv == -1){
				free(hdr);
				free(segment);
				return -1;
			}

			retv = send_segment(sockfd, fdctx->mip_addr, segment, seg_size);
			if(retv == -1){
				free(hdr);
				free(segment);
				return retv;	
			}

			free(hdr);
			free(segment);
			win->lfs = i;
		}

	}

	win->lar = -1;

	fprintf(stderr, "nof after sending window: %d\n", win->nof);
	fprintf(stderr, "lar after sending window: %d\n", win->lar);
	fprintf(stderr, "lsf after sending window: %d\n", win->lfs);

	return 0;
}

int timeout_event(int sockfd, fdcontext_t *fdctx, int timeout){
	int retv, i;
	sender_t *win = fdctx->s_win;
	fragment_t *temp;

	for(i=0; i<win->nof; i++){
		temp = win->fragments[i];

		fprintf(stderr, "resent: %d\n", temp->resent);
		
		if(temp->timerfd == fdctx->fd){

			if(temp->resent >= 3){
				fprintf(stderr, "Segment %d failed to reach destination 3 times!\n", i);
				return 0;
			}

			retv = start_timer(temp->timerfd, timeout);
			if(retv == -1)
				return -1;

			retv = send_segment(sockfd, fdctx->mip_addr, temp->data, temp->data_size);
			if(retv == -1)
				return -1;

			temp->resent++;
			break;
		}

	}

	return 1;
}

int find_port(uint16_t port, fdcontext_t *array[], int arrlen){
	int i;
	fdcontext_t *temp;

	for(i=0; i<arrlen; i++){
		temp = array[i];

		if(temp != NULL){
			if(temp->port == port)
				return i;
		}

	}
		
	fprintf(stderr, "port not found!\n");
	return -1;	
}

int receiver_check(uint16_t mip_addr, header_t *hdr){
	fdcontext_t *fdctx;
	receiver_t *win;
	int index = find_port(hdr->port, fd_list, FDMAX);

	// application listening on port?
  if(index != -1){
	  fdctx = fd_list[index];
	  fdctx->mip_addr = mip_addr;
	  win = fdctx->r_win;

	  // already received fragment?
	  if(win->fragments[hdr->seqnum] != NULL){
	  	return 0;
	  }
	  else if(hdr->seqnum < win->lfr){
	  	return 0;
	  }

	 	// fragment in window?
		if(win->lfr < hdr->seqnum && hdr->seqnum < win->laf){
			return 1;
		}

	}

	return -1;
}

void save_fragment(receiver_t *win, header_t *hdr, char *segment, int seg_size){
	uint16_t filesize, fragsize;
	fragment_t *new;

	fprintf(stderr, "inside save_fragment(): seg_size: %d\n", seg_size);

	fragsize = seg_size - TP_SIZE - hdr->pl;
	fprintf(stderr, "fragment size: %d\n", fragsize);

	new = malloc(sizeof(fragment_t) + fragsize);
	memset(new, 0, sizeof(fragment_t) + fragsize);

	new->data_size = fragsize;
	memcpy(new->data, &segment[TP_SIZE], fragsize);

	win->fragments[hdr->seqnum] = new;

	if(hdr->seqnum == 0){
		memcpy(&filesize, &segment[TP_SIZE], sizeof(uint16_t));

		fprintf(stderr, "filesize: %d\n", filesize);

		win->nof = num_of_fragments(filesize);

		fprintf(stderr, "Number of fragments: %d\n", win->nof);
	}

}

int update_receiver(receiver_t *win, uint16_t seqnum){
	fprintf(stderr, "lfr: %d\n", win->lfr);
	fprintf(stderr, "laf: %d\n", win->laf);

	// first frame in window?
	if(seqnum == win->lfr+1){
		win->laf = win->lfr + WIN_SIZE;
		win->lfr++;
	}
	// last fragment?
	if(win->lfr == win->nof-1){
		return 1;
	}

	return 0;
}

int in_window(header_t *hdr, sender_t *win){
	fragment_t *temp;

	if(win != NULL){
		// in window?
		if(hdr->seqnum > win->lar && hdr->seqnum <= win->lfs){
			temp = win->fragments[hdr->seqnum];
			temp->ack = 1;
			return 1;
		}
	}

	return 0;
}

int update_sender(int sockfd, fdcontext_t *fdctx, int timeout){
	int retv;
	fragment_t *temp;
	sender_t *win = fdctx->s_win;
	// last fragment of file?
	if(win->lar == win->nof - 1){
		fprintf(stderr, "file sent!\n");
		return 1;
	}

	temp = win->fragments[win->lar + 1];
	// first fragment in windows ack'ed?
	if(temp->ack == 1){
		win->lar++;
		win->fragments[win->lar] = NULL;
		close(temp->timerfd);
		free(temp);

		retv = move_window(sockfd, fdctx, timeout);
	}

	return retv;
}

int move_window(int sockfd, fdcontext_t *fdctx, int timeout){
	int retv, i;
	char *hdr, *segment;
	uint16_t seg_size;
	sender_t *win = fdctx->s_win;
	fragment_t *temp;
	fdcontext_t *timer;

	i = win->lar + WIN_SIZE;

	fprintf(stderr, "lar before sending window: %d\n", win->lar);
	fprintf(stderr, "lfs before moving window: %d\n", win->lfs);
	fprintf(stderr, "win->lfs - win->lar = %d\n", win->lfs - win->lar);
	fprintf(stderr, "i+1 = %d\n", i+1);
	fprintf(stderr, "nof: %d\n", win->nof);

	if(win->lfs - win->lar <= WIN_SIZE){
		if(i > win->nof - 1){
			return 0;
		}

		temp = win->fragments[i];

		hdr = create_tphdr(temp->data_size, fdctx->port, i);
		segment = add_hdr(hdr, TP_SIZE, temp->data, temp->data_size);
		seg_size = temp->data_size + TP_SIZE;

		timer = malloc(sizeof(fdcontext_t));
		init_fdctx(timer, 0, fdctx->port, fdctx->mip_addr, win, NULL, 1);

		retv = add_timer(timer);
		if(retv == -1){
			free(hdr);
			free(segment);
			return -1;
		}

		// adding to fragment
		temp->timerfd = timer->fd;
		temp->resent = 0;

		retv = start_timer(timer->fd, timeout);
		if(retv == -1){
			free(hdr);
			free(segment);
			return -1;
		}

		retv = send_segment(sockfd, fdctx->mip_addr, segment, seg_size);
		if(retv == -1){
			free(hdr);
			free(segment);
			return retv;	
		}

		// test print
		fprintf(stderr, "Number of bytes sent: %d\n", retv);
		// end of test

		free(hdr);
		free(segment);
		win->lfs++;
	}

	return 0;
}

/*
INPUT PARAMETERS
  - writefds: fd_set with applications ready to read
  - fdmax: highest file descriptor number of the master file descriptors set
  - mip_addr: source address of the message to be sent
  - data: message to be sent
  - data_len: length of message

This function begins by looking applications wanting to read in 'writefds'. If
found, 'mip_addr', 'data' and 'data_len' is stored in a msghdr and sent to the
application with sendmsg(). If a reading application is not found within 1 
second, 0 is returned. If an error occur in select() or sendmsg(), -1 is 
returned.
*/
// int send_msg(fd_set *writefds, int fdmax, int mip_addr, char *data, int data_len){
//   int retv;
//   struct timeval timeout = { 0 };

//   timeout.tv_sec = 1;
//   timeout.tv_usec = 0;

//   retv = select(fdmax+1, NULL, writefds, NULL, &timeout);
//   if(retv == -1){
//     perror("send_msg(): select()");
//     return -1;
//   }
//   else if(retv == 0){
//     fprintf(stderr, "TIMEOUT - NO APPLICATIONS READY TO RECEIVE MESSAGE\n");
//     return 0;
//   }

//   int i;
//   for(i=0; i<=fdmax; i++){
//     if(FD_ISSET(i, writefds)){ 
//       ssize_t retv2;

//       struct iovec iov[2];
//       iov[0].iov_base = &mip_addr;
//       iov[0].iov_len = sizeof(mip_addr);
//       iov[1].iov_base = data;
//       iov[1].iov_len = data_len;

//       struct msghdr msg = { 0 };
//       msg.msg_iov = iov;
//       msg.msg_iovlen = 2;

//       retv2 = sendmsg(i, &msg, 0);
//       if(retv2 == -1){
//         perror("main(): sendmsg()");
//         return -1;
//       }
//     }
//   }

//   return 0;
// }