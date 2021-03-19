CC     := gcc
CFLAGS := -g2 -Wall
LDLIBS := -lm

.PHONY: all clean

all: sys lib

sys: main.c
	$(CC) $(CFLAGS) main.c -o sys $(LDLIBS)

lib: main.c
	$(CC) $(CFLAGS) -DLIB main.c -o lib $(LDLIBS)

clean:
	$(RM) sys lib
