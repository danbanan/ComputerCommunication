#include "router.h"
#include "debug.h"

int debug;

int main(int argc, char *argv[]){
	int retv;
	int fdmax = 0;
	fd_set master, readfds;

	retv = handle_argv(argc, argv);
	if(retv == -1)
		exit(EXIT_SUCCESS);

	FD_ZERO(&readfds);
	FD_ZERO(&master);

/* ------------------------------ FORWARD SOCKET --------------------------- */

	int forward = new_socket(argv[optind]);
	if(forward == -1){
		exit(EXIT_FAILURE);
	}

	fdmax = new_fdmax(forward, fdmax);
	FD_SET(forward, &master);

/* ------------------------------ ROUTING SOCKET --------------------------- */

	int routing = new_socket(argv[optind+1]);
	if(routing == -1){
		close_all(&master, fdmax);
		exit(EXIT_FAILURE);
	}

	fdmax = new_fdmax(routing, fdmax);
	FD_SET(routing, &master);

/* ------------------- STORING LOCAL INTERFACES IN DVR TABLE --------------- */

	int update_size = 0;

	DLOG("receiving routing update from MIP daemon");
	char *first_update = recv_update(routing, &update_size);
	if(first_update == NULL){
		free(first_update);
		close_all(&master, fdmax);
		exit(EXIT_FAILURE);
	}

	struct route *dvr_table = NULL;

	int count = 0;
	while(count < update_size){
		struct route *new = malloc(sizeof(struct route));
		memset(new, 0, sizeof(struct route));
		new->mip_end = first_update[count++];

		dvr_table = add_route(dvr_table, new);
	}

	free(first_update);

	print_route(dvr_table);

	int start_len = update_size;
	// the number of neighbors you have is equal to the number of local MIP 
	// addresses available
	uint8_t neighbors[start_len];
	memset(neighbors, 0, sizeof(neighbors));

	struct timeval timers[start_len];

	count = 0;
	while(count < start_len){
		gettimeofday(&timers[count], NULL);
		count++;
	}

/* -------------------------------- PIPES FD ------------------------------- */
	
	int pipe_fd[2];

	retv = pipe(pipe_fd);
	if(retv == -1){
		perror("main(): pipe()");
		free_routes(dvr_table);
		close_all(&master, fdmax);
		exit(EXIT_FAILURE);		
	}

	fdmax = new_fdmax(pipe_fd[0], fdmax);
	FD_SET(pipe_fd[0], &master);
 
/* ------------------------------ MAIN LOOP -------------------------------- */
	int child_pid = 0;

	if((child_pid = fork())){
		// parent process
		close(pipe_fd[1]); // write-end of pipe

		for(;;){

			count = 0;
			while(count < start_len){

				if(neighbors[count] != 0){

					retv = timeout(timers[count].tv_sec);
					if(retv == -1){

						int poison_size = 0;
						char *poison = create_poison(dvr_table, neighbors[count], \
																																&poison_size);
						
						DLOG("sending dead-link update");
						retv = send_update(routing, poison, poison_size);
						if(retv == -1){
							free(poison);
							free_routes(dvr_table);
							close_all(&master, fdmax);
							kill(child_pid, SIGTERM);
							wait(NULL);
							exit(EXIT_FAILURE);
						}

						free(poison);
					}

				}

				count++;
			}

			readfds = master;

			DLOG("looking for activity in socket set...");
			retv = select(fdmax+1, &readfds, NULL, NULL, NULL);
			if(retv == -1){
				perror("main(): select()");
				free_routes(dvr_table);
				close_all(&master, fdmax);
				kill(child_pid, SIGTERM);
				wait(NULL);
				exit(EXIT_FAILURE);
			}
			DLOG("found activity\n");

			int i;
			for(i=0; i<=fdmax; i++){

				if(FD_ISSET(i, &readfds)){

					if(i == forward){

						uint8_t mip_req = 0;
						
						DLOG("receiving route request from MIP daemon");
						retv = recv_request(i, &mip_req);
						if(retv == -1){
							free_routes(dvr_table);
							close_all(&master, fdmax);
							kill(child_pid, SIGTERM);
							wait(NULL); // waiting for any child process to terminate
							exit(EXIT_FAILURE);
						}

						uint16_t next = get_next(dvr_table, mip_req);

						DLOG("sending route to MIP daemon");
						retv = send_next(i, next);
						if(retv == -1){
							free_routes(dvr_table);
							close_all(&master, fdmax);
							kill(child_pid, SIGTERM);
							wait(NULL); // waiting for any child process to terminate
							exit(EXIT_FAILURE);	
						}

					}
					else if(i == routing){

						update_size = 0;

						DLOG("receiving routing update from MIP daemon");
						char *update = recv_update(i, &update_size);
						if(update == NULL){
							free_routes(dvr_table);
							close_all(&master, fdmax);
							kill(child_pid, SIGTERM);
							wait(NULL); // waiting for any child process to terminate
							exit(EXIT_FAILURE);
						}

						int j;
						// new neighbor?
						for(j=0; j<start_len; j++){
							if(neighbors[j] == (uint8_t)update[0]){
								gettimeofday(&timers[j], NULL);
								break;
							}
							else if(neighbors[j] == 0){
								neighbors[j] = (uint8_t)update[0];
								gettimeofday(&timers[j], NULL);
								break;
							}
						}

						DLOG("updating DVR table");
						dvr_table = update_table(dvr_table, update, update_size);

						free(update);
					}
					else{ //read-end pipe

						char *update;
						int update_size = 0;

						uint8_t byte = 0;
						// flushing write buffer of pipe_fd
						retv = read(i, &byte, sizeof(uint8_t));
						if(retv == -1){
							perror("main(): read()");
							free_routes(dvr_table);
							close_all(&master, fdmax);
							kill(child_pid, SIGTERM);
							wait(NULL); // waiting for any child process to terminate
							exit(EXIT_FAILURE);	
						}

						// neighbors in dvr_table?
						if(start_len < get_length(dvr_table, 255)){

							int j;
							for(j=0; j<start_len; j++){

								if(neighbors[j] != 0){

									update = create_update(dvr_table, neighbors[j], &update_size);

									DLOG("sending routing update to MIP daemon");
									retv = send_update(routing, update, update_size);
									if(retv == -1){
										free(update);
										free_routes(dvr_table);
										close_all(&master, fdmax);
										exit(EXIT_FAILURE);
									}

									free(update);
								}

							}

						}
						else{

							update = create_update(dvr_table, 255, &update_size);

							DLOG("sending routing update to MIP daemon");
							retv = send_update(routing, update, update_size);
							if(retv == -1){
								free(update);
								free_routes(dvr_table);
								close_all(&master, fdmax);
								exit(EXIT_FAILURE);
							}

							free(update);
						}

					}

				}

			}

		}

	}
	else{
		// child process
		signal(SIGTERM, sighandler);
		int write_fd = pipe_fd[1];
		close(pipe_fd[0]); //read_end of pipe

		uint8_t n = 1;

		for(;;){
			sleep(10);
			
			retv = write(write_fd, &n, sizeof(uint8_t));
			if(retv == -1){
				perror("child: main(): send()");
				close(write_fd);
				exit(EXIT_FAILURE);
			}

			n++;
		}
	}

	return 0;
}