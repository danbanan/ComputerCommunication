#include "router.h"
#include "debug.h"

/*
INPUT PARAMETERS
  - arg_req: number of arguments requested to run the program
  - argc: number of arguments given when running the program
  - argv: arguments given when running the program

This function prints out how to run the program if the number of arguments 
given is not equal to the number of arguments required and returns 0.
*/
int proper_usage(int arg_req, int argc, char *argv[]){
  if(argc != arg_req){
    fprintf(stderr, "USAGE: %s [-d] <Forwarding_socket> <Routing_socket> \n", \
                              																				argv[0]);
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
  - debug: debug-print boolean 0/1

This function handles option flags in the cmd-line and makes sure that the user
starts the program correctly.
*/
int handle_argv(int argc, char *argv[]){
  opterr = 0; //to make getopt not print error message
  int retv = getopt(argc, argv, "d");
  if(retv == 'd'){

    if(!proper_usage(4, argc, argv))
      return -1;
    
    debug = 1;
  }
  else if(!proper_usage(3, argc, argv)){
    return -1;
  }

  return 0;
}

/*
INPUT PARAMETER
	- signum: signal that were caught

This fuction prints out 'signum' and exits.
*/
void sighandler(int signum){
	fprintf(stderr, "Caught signal: %d\n", signum);
	exit(EXIT_FAILURE);
}

/*
OUTPUT PARAMETERS
	- list: linked list of route structs

This function frees every allocated node in 'list'.
*/
void free_routes(struct route *list){
	struct route *temp = list;

	while(list != NULL){
		temp = list;
		list = list->next;
		free(temp);
	}
}

/*
INPUT PARAMETERS
  - master: file descriptor set
  - fdmax: highest file descriptor number in 'master'

This function closes all sockets in fd_set 'master'.
*/
void close_all(fd_set *master, int fdmax){
	int i;
	for(i=0; i<fdmax; i++){
		if(FD_ISSET(i, master)){
			close(i);
			FD_CLR(i, master);
		}
	}
}

/*
INPUT PARAMETER
	- sockpath: path of the socket to be made

OUTPUT PARAMETER
	- sockfd: new unix socket

This function creates a new unix socket with sockpath and connects it to a 
listening socket before returning sockfd. -1 is returned if an error occur.
*/
int new_socket(char *sockpath){
	int retv;
	struct sockaddr_un my_addr = { 0 };

	int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if(sockfd == -1){
		perror("new_socket()");
		return -1;
	}

	my_addr.sun_family = AF_UNIX;
	memcpy(my_addr.sun_path, sockpath, strlen(sockpath));

	retv = connect(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr));
	if(retv == -1){
		perror("new_socket()");
		return -1;
	}

	return sockfd;
}

/*
INPUT PARAMETER
  - sockfd: file descriptor number

OUTPUT PARAMETER
  - fdmax: highest number of the file descriptors numbers in fd_set master

This function updates fdmax to sockfd if sockfd is bigger than fdmax.
*/
int new_fdmax(int sockfd, int fdmax){
  if(sockfd > fdmax){
  	return sockfd;
  }
  return fdmax;
}

/*
INPUT PARAMETERS
	- list: linkes list of route structs
	- new: new route struct to be added to list

This function adds the route struct 'new' to 'list' and returns the new list.
*/
struct route *add_route(struct route *list, struct route *new){
	struct route *temp = list;

	if(temp != NULL){
		while(temp->next != NULL){
			temp = temp->next;
		}
		temp->next = new;

		return list;
	}

	return new;
}

/*
INPUT PARAMETERS
	- list: linked list of route structs
	- mip_addr: MIP address of the route that will be removed from list

This function removes a node from 'list' with the same 'mip_next' address as 
'mip_addr'. 'list' is returned after removing a node or iteration of 'list' is
complete.
*/
struct route *remove_route(struct route *list, uint8_t mip_addr){
	struct route *temp = list;
	struct route *prev = NULL;

	if(temp != NULL){
		while(temp->next != NULL){
			if(temp->mip_end == mip_addr){
				// first node of the list?
				if(temp->mip_end == list->mip_end){
					list = list->next;
					free(temp);

					return list;
				}
				prev->next = temp->next;
				free(temp);

				return list;
			}
			prev = temp;
			temp = temp->next; 
		}

		// last node of the list
		if(temp->mip_end == mip_addr){
			// last and only node of the list?
			if(temp->mip_end == list->mip_end){
				free(list);
				return NULL;
			}
			prev->next = NULL;
			free(temp);

			return list;
		}
	}

	return list;
}

/*
INPUT PARAMETER
	- list: a linked list of rute structs

This function prints out every route in 'list' to the terminal.
*/
void print_route(struct route *list){
	struct route *temp = list;

	char s[15] = { 0 };
	memset(s, '-', 14);

	char s2[60] = { 0 };
	memset(s2, '-', 59);

	fprintf(stderr, "%-15s%s%-15s\n", s, "DISTANCE VECTOR ROUTING TABLE", s);
	fprintf(stderr, "%-20s%-20s%-20s\n", "Destination", "Cost", "Next Jump");

	while(temp != NULL){
		fprintf(stderr, "%-20d%-20d%-20d\n", temp->mip_end, temp->cost, \
																															temp->mip_next);

		temp = temp->next;
	}

	fprintf(stderr, "%s\n", s2);
}

/*
INPUT PARAMETER
	- table: linked list of route structs
	- mip_addr: MIP address

OUTPUT PARAMETER
	- count: length of 'list'

This function finds the length of 'list' if 'list' didn't have any structs with
'mip_next' equal to 'mip_addr'.
*/
int get_length(struct route *list, int mip_addr){
	int count = 0;
	struct route *temp = list;

	while(temp != NULL){

		if(temp->mip_next != mip_addr){
			count++;
		}

		temp = temp->next;
	}

	return count;
}

/*
INPUT PARAMETER
	- list: linked list of route structs
	- mip_addr: MIP source address of update

INPUT-OUTPUT PARAMETER
	-	update_size: where size of update is stored

OUTPUT PARAMETER
	- update_ptr: update to be sent

This function creates an update returns it.

Update structure: | src |dst|cost|dst|cost|...| 255 |
src is the head, and 255 is the tale of the update.

update[count++]: count is returned before incrementing
*/
char *create_update(struct route *list, int mip_addr, int *update_size){
	int update_len = get_length(list, mip_addr);
	update_len = (update_len * 2) + 2; 

	unsigned char update[update_len]; 
	memset(update, 0, sizeof(update));

	struct route *temp = list;
	int count = 0;

	// destination MIP address of update
	update[count++] = mip_addr;

	while(temp != NULL){
		// Split horizon test
		if(temp->mip_next != mip_addr){
			// in array and not last index?
			if(count < (update_len-1)){
				update[count++] = temp->mip_end;
				update[count++] = temp->cost;
			}

			// last index?
			if(count == (update_len-1)){
				update[count] = 255;
				break;
			}

		}

		temp = temp->next;			
	}

	*update_size = sizeof(update);

	char *update_ptr = malloc(sizeof(update));
	memcpy(update_ptr, update, sizeof(update));

	return update_ptr;
}

/*
INPUT PARAMETERS
	- update: update to be printed
	- update_size: size of update

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

/*
INPUT PARAMETERS
	- sockfd: socket for routing communication
	- update: DVR table update
	- update_size: size of update

This function send a DVR table update to MIP daemon on the same host through 
'sockfd'. If the size of the update is not a multiple of 4, padding is added to
the update. If an error occur, -1 is returned.
*/
int send_update(int sockfd, char *update, int update_size){
	ssize_t retv;

	int buf_size = update_size;
	int padding = update_size % 4;

  if(padding != 0){
    buf_size = update_size + 4 - padding;
  }

  char buf[buf_size];
  memset(buf, 0, buf_size);

  memcpy(buf, update, update_size);

 	retv = send(sockfd, buf, buf_size, 0);
	if(retv == -1){
		perror("send_update(): send()");
		return -1;
	}

	return 0;
}

/*
INPUT PARAMETER
	- sockfd: socket file descriptor

INPUT-OUTPUT PARAMETER
	- update_size: where the number of bytes received/update size is stored

OUTPUT PARAMETER
	- update_ptr: received DVR table update

This function recieves a DVR update from 'sockfd', and returns the update. 
'update_size' gets changed to the number of bytes received. If an error occur
NULL is returned.
*/
char *recv_update(int sockfd, int *update_size){
	ssize_t retv;
	char update[BUF_SIZE] = { 0 };

	retv = recv(sockfd, update, BUF_SIZE, 0);
  if(retv == 0){
    DLOG("connection closed!\n");
    return NULL;
  }
	else if(retv == -1){
		perror("recv_update(): recv()");
		return NULL;
	}

	char *update_ptr = malloc(retv);
	memcpy(update_ptr, update, retv);

	*update_size = retv;

	return update_ptr;
}

/*
INPUT PARAMETERS
	- update: DVR table update
	- update_size: size of DVR table

INPUT-OUTPUT PARAMETER
	- table: linked list of route structs

This function updates the DVR table 'table' with following scenarious in mind:
 - dead link in next hop?
 - cheaper route?
 - new destination?
 
'table' is returned.
*/
struct route *update_table(struct route *table, char *update, int update_size){
	uint8_t dst, src, cost, buf[update_size];
	int in_table;
	int update_occur = 0;

	memcpy(buf, update, update_size);

	int count = 0;
	struct route *temp;

	src = buf[count++];

	while(count < update_size){
		// end of update?
		if(buf[count] == 255){
			break;
		}

		dst = buf[count++];
		cost = buf[count++];

		in_table = 0;

		temp = table;

		while(temp != NULL){
			if(temp->mip_end == dst){
				in_table = 1;
			}

			// is mip_next a dead link?
			if(cost == 16 && temp->mip_next == src && temp->mip_end == dst){
				table = remove_route(table, dst);
				update_occur = 1;
			}
			// cheaper route?
			else if((cost+1) < temp->cost && temp->mip_end == dst){
				temp->cost = cost + 1;
				temp->mip_next = src;
				update_occur = 1;
			}

			temp = temp->next;
		}

		// new route with a living link?
		if(cost != 16 && !in_table){
			struct route *new = malloc(sizeof(struct route));
			new->mip_end = dst;
			new->cost = cost + 1;
			new->mip_next = src;
			new->next = NULL;

			table = add_route(table, new);
			update_occur = 1;
		}

	}

	if(update_occur){
		print_route(table);
	}

	return table;
}

int recv_request(int sockfd, uint8_t *mip_req){
	int retv;

	retv = recv(sockfd, mip_req, sizeof(uint8_t), 0);
	if(retv == -1){
		perror("recv_request(): recv()");
		return -1;
	}

	return 0;
}

uint16_t get_next(struct route *list, uint8_t mip_req){
	uint16_t next = 0;

	struct route *temp = list;
	while(temp != NULL){

		if(temp->mip_end == mip_req){
			next = temp->mip_end;
			next = (next << 8) | temp->mip_next;
		}

		temp = temp->next;
	}

	return next;
}

int send_next(int sockfd, uint16_t next){
	int retv;
	uint16_t buf = next;

	retv = send(sockfd, &buf, sizeof(buf), 0);
	if(retv == -1){
		perror("send_next(): send()");
		return -1;
	}

	return 0;
}

int timeout(time_t tv_sec){
	long time_spent;
	struct timeval end = { 0 };

	gettimeofday(&end, NULL);

  time_spent = end.tv_sec - tv_sec;

  if(time_spent > 30){
  	return -1;
  }

  return 0;
}

char *create_poison(struct route *table, uint8_t mip_addr, int *size){
	int dead_len = get_length(table, 255);
	dead_len = (dead_len - get_length(table, mip_addr));
	dead_len = (dead_len * 2) + 2;

	uint8_t dead[dead_len];
	dead[0] = 255;
	dead[dead_len-1] = 255;

	int count = 1;
	struct route *temp = table;
	while(temp != NULL){

		if(temp->mip_next == mip_addr){
			dead[count++] = temp->mip_end;
			dead[count++] = 16;
		}

		temp = temp->next;
	}

	*size = sizeof(dead);

	char *dead_ptr = malloc(sizeof(dead));
	memcpy(dead_ptr, dead, sizeof(dead));

	return dead_ptr;
}