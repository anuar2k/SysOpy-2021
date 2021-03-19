CC     := gcc
CFLAGS := -g2 -Wall

.PHONY: all clean

all: sys lib

sys: main.c
	$(CC) $(CFLAGS) main.c -o sys

lib: main.c
	$(CC) $(CFLAGS) -DLIB main.c -o lib

clean:
	$(RM) sys lib
