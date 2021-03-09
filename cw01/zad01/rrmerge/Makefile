NAME    := librrmerge
SNAME   := $(NAME).a
DNAME   := $(NAME).so
CC      := gcc
CFLAGS  := -Wall -g2
ARFLAGS := rcs
DIST    := /usr/local

.PHONY: all clean install uninstall

all: librrmerge.a librrmerge.so

install: librrmerge.so
	mkdir -p $(DIST)/include/rrmerge $(DIST)/lib/rrmerge
	cp rrmerge.h $(DIST)/include/rrmerge/rrmerge.h
	cp ptr_vector.h $(DIST)/include/rrmerge/ptr_vector.h
	cp librrmerge.so $(DIST)/lib/rrmerge/librrmerge.so

uninstall:
	rm -rf $(DIST)/include/rrmerge $(DIST)/lib/rrmerge

librrmerge.a: rrmerge_static.o ptr_vector_static.o
	$(AR) $(ARFLAGS) librrmerge.a rrmerge_static.o ptr_vector_static.o

librrmerge.so:
	$(CC) $(CFLAGS) rrmerge.c ptr_vector.c -fPIC -shared -o librrmerge.so

rrmerge_static.o:
	$(CC) $(CFLAGS) rrmerge.c -c -o rrmerge_static.o

ptr_vector_static.o:
	$(CC) $(CFLAGS) ptr_vector.c -c -o ptr_vector_static.o

clean:
	$(RM) *.a *.o *.so
