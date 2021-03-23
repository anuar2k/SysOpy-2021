CC     := gcc
CFLAGS := -g2 -Wall
LDLIBS := -lrrmerge

.PHONY: all clean

all: main

main: main.c
	$(CC) $(CFLAGS) main.c -o main $(LDLIBS)

clean:
	$(RM) main
