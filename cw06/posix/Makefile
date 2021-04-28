CC     := gcc
CFLAGS := -g2 -Wall

.PHONY: all clean

all: server client

server: server.c chat.h
	$(CC) $(CFLAGS) server.c -o server

client: client.c chat.h
	$(CC) $(CFLAGS) client.c -o client

clean:
	$(RM) server client
