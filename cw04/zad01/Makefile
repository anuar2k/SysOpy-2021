CC          := gcc
CFLAGS      := -g2 -Wall
TEST_SIGNAL ?= SIGUSR1

.PHONY: all clean

all: main to_exec

main: main.c
	$(CC) $(CFLAGS) -DTEST_SIGNAL=$(TEST_SIGNAL) main.c -o main

to_exec: to_exec.c
	$(CC) $(CFLAGS) -DTEST_SIGNAL=$(TEST_SIGNAL) to_exec.c -o to_exec

clean:
	$(RM) main to_exec
