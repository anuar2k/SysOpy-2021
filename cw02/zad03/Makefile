CC     := gcc
CFLAGS := -g2 -Wall
LDLIBS := -lm

.PHONY: all test clean

all: sys lib

sys: main.c
	$(CC) $(CFLAGS) main.c -o sys $(LDLIBS)

lib: main.c
	$(CC) $(CFLAGS) -DLIB main.c -o lib $(LDLIBS)

test: sys lib
	/usr/bin/time -p -o sys_result.txt ./sys 1>/dev/null
	/usr/bin/time -p -o lib_result.txt ./lib 1>/dev/null

clean:
	$(RM) sys lib *result.txt a.txt b.txt c.txt
