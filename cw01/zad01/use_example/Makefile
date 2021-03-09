CC     := gcc
CFLAGS := -Wall -g2
LDLIBS := -lrrmerge

.PHONY: all clean

all: main

main:
	$(CC) $(CFLAGS) main.c $(LDLIBS) -o main

clean:
	$(RM) main