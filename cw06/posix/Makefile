CC     := gcc
CFLAGS := -g2 -Wall
LDLIBS := -lrt

.PHONY: all clean

all: server client

server: server.c chat.h
	$(CC) $(CFLAGS) server.c -o server $(LDLIBS)

client: client.c chat.h
	$(CC) $(CFLAGS) client.c -o client $(LDLIBS)

clean:
	$(RM) server client
