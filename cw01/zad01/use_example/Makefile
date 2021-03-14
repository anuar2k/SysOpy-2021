CC     := gcc
CFLAGS := -Wall -g2
LDLIBS := -lrrmerge

.PHONY: all clean

all: main_static main_shared

main_static: main.c
	$(CC) $(CFLAGS) -static main.c -I../rrmerge -L../rrmerge $(LDLIBS) -o main_static

main_shared: main.c
	$(CC) $(CFLAGS) main.c $(LDLIBS) -o main_shared

clean:
	$(RM) main_static main_shared