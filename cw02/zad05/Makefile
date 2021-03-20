CC     := gcc
CFLAGS := -g2 -Wall

.PHONY: all test clean

all: sys lib

sys: main.c
	$(CC) $(CFLAGS) main.c -o sys

lib: main.c
	$(CC) $(CFLAGS) -DLIB main.c -o lib

test: sys lib
	/usr/bin/time -p -o sys_result.txt ./sys test/test.txt split.txt 1>/dev/null
	/usr/bin/time -p -o lib_result.txt ./lib test/test.txt split.txt 1>/dev/null

clean:
	$(RM) sys lib *result.txt split.txt
