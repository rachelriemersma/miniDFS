CC = gcc
CFLAGS = -Wall -pthread

all: server client

server: server/server.c
	$(CC) $(CFLAGS) -o server server/server.c

client: client/client.c
	$(CC) $(CFLAGS) -o client client/client.c

clean:
	rm -f server client