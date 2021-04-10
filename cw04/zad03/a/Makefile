CC     := gcc
CFLAGS := -g2 -Wall

.PHONY: all clean

all: sender catcher

sender: sender.c
	$(CC) $(CFLAGS) sender.c -o sender

catcher: catcher.c
	$(CC) $(CFLAGS) catcher.c -o catcher

clean:
	$(RM) sender catcher
