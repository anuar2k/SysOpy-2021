CC     := gcc
CFLAGS := -g2 -Wall

.PHONY: all clean

all: main

main: main.c ptr_vector.h ptr_vector.c
	$(CC) $(CFLAGS) main.c ptr_vector.c -o main

clean:
	$(RM) main
