CC=gcc
AR=ar
CFLAGS=-I.
AFLAGS=rcs
LIBNM=libtaskqueue.a
DEPS=taskqueue.h
OBJ=taskqueue.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) -lpthread

library: $(OBJ)
	$(AR) $(AFLAGS) $(LIBNM) $^
