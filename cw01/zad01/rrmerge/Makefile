CC      := gcc
CFLAGS  := -Wall -g2
ARFLAGS := rcs
DIST    := /usr

.PHONY: all clean install uninstall

all: librrmerge.a librrmerge.so

install: librrmerge.so
	mkdir -p $(DIST)/include $(DIST)/lib
	cp rrmerge.h $(DIST)/include/rrmerge.h
	cp rrmerge_ptr_vector.h $(DIST)/include/rrmerge_ptr_vector.h
	cp librrmerge.so $(DIST)/lib/librrmerge.so

uninstall:
	$(RM) $(DIST)/include/rrmerge.h 
	$(RM) $(DIST)/include/rrmerge_ptr_vector.h
	$(RM) $(DIST)/lib/librrmerge.so

librrmerge.a: rrmerge_static.o rrmerge_ptr_vector_static.o
	$(AR) $(ARFLAGS) librrmerge.a rrmerge_static.o rrmerge_ptr_vector_static.o

librrmerge.so:
	$(CC) $(CFLAGS) rrmerge.c rrmerge_ptr_vector.c -fPIC -shared -o librrmerge.so

rrmerge_static.o:
	$(CC) $(CFLAGS) rrmerge.c -c -o rrmerge_static.o

rrmerge_ptr_vector_static.o:
	$(CC) $(CFLAGS) rrmerge_ptr_vector.c -c -o rrmerge_ptr_vector_static.o

clean:
	$(RM) *.a *.o *.so
