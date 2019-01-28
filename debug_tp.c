#include "tp.h"

void print_hdr(header_t *hdr){
	fprintf(stderr, "PL: %d\n", hdr->pl);
	fprintf(stderr, "Port: %d\n", hdr->port);
	fprintf(stderr, "Sequence number: %d\n", hdr->seqnum);
}

void print_bit_values(char *data, int num){
	int i;
	uint8_t buf[num];
	
	memcpy(buf, data, num);

	for(i=0; i<num; i++){
		fprintf(stderr, "[%d]: %d\n", i, buf[i]);
	}

}