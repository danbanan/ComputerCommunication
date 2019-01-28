CC = gcc
CFLAGS = -g -Wall -Wextra -Wpedantic -std=gnu99
BINARIES =  mip_daemon ping_client ping_server router mip_tp

all: $(BINARIES)

test: $(TBINARIES)

ping_client: ping_client.c app_func.c sockets.c app.h sock.h debug.h
	$(CC) $(CFLAGS) ping_client.c app_func.c sockets.c -o ping_client

ping_server: ping_server.c app_func.c sockets.c app.h sock.h debug.h
	$(CC) $(CFLAGS) ping_server.c app_func.c sockets.c -o ping_server

mip_daemon: mip_daemon.c daemon_func.c sockets.c debug_daemon.c daemon.h debug.h sock.h
	$(CC) $(CFLAGS) mip_daemon.c daemon_func.c sockets.c debug_daemon.c -o mip_daemon

router: router_main.c router_func.c router.h debug.h
	$(CC) $(CFLAGS) router_main.c router_func.c -o router

mip_tp: mip_tp.c sub_tp.c sockets.c debug_tp.c tp.h sock.h
	$(CC) $(CFLAGS) mip_tp.c sub_tp.c sockets.c debug_tp.c -o mip_tp

unlink:
	rm path*

clean:
	rm $(BINARIES)

