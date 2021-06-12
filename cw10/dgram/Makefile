CC     := gcc
CFLAGS := -g2 -Wall

.PHONY: all clean

all: server client

server: server.c tictactoe.h
	$(CC) $(CFLAGS) server.c -o server -pthread

client: client.c tictactoe.h
	$(CC) $(CFLAGS) client.c -o client -pthread

clean:
	$(RM) server client
