CC     := gcc
CFLAGS := -Wall -g2
LDLIBS := -lrrmerge
LIB    := -L/usr/local/lib/rrmerge
INC    := -I/usr/local/include/rrmerge

.PHONY: all clean

all: main

main:
	$(CC) $(CFLAGS) main.c $(INC) $(LIB) $(LDLIBS) -o main

clean:
	$(RM) main