CC=gcc
AR=ar
CFLAGS=-I.
AFLAGS=rcs
LIBNM=libtaskqueue.a
DEPS=taskqueue.h
OBJ=taskqueue.o
LIBS=-lpthread

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

library: $(OBJ)
	$(AR) $(AFLAGS) $(LIBNM) $^

.PHONY: clean

clean:
	rm -f *.o *.a
