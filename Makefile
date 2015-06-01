CC		:= gcc
AR		:= ar
AFLAGS		:= rcs
SRCDIR		:= `pwd`
CFLAGS		:= -Wall -Wno-unused-function -I.
LIBNM		:= libtaskqueue.a
DEPS		:= taskqueue.h
OBJ		:= taskqueue.o
LIBS		:= -lpthread
TSTDIR		:= testsuite

all: buildlib

lib: $(OBJ)
	@$(AR) $(AFLAGS) $(LIBNM) $^

%.o: %.c $(DEPS)
	@$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

.PHONY: clean check

clean:
	@$(RM) -f *.o *.a

extraclean: clean
	@$(MAKE) -C $(TSTDIR) clean

buildtests:
	@$(MAKE) -C $(TSTDIR) build

check: lib
	@$(MAKE) -C $(TSTDIR) $@
