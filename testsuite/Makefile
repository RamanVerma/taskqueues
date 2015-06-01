CC		:= gcc
SRCDIR		:= $(shell dirname `pwd`)
CFLAGS		:= -g
INCDIR		:= -I $(SRCDIR)
LIBS		:= -L $(SRCDIR) -lpthread -ltaskqueue
TSTFILES	:= testsuite.c
TSTEXEC		:= tests

all: build

build: lib $(TSTFILES)
	@$(CC) $(CFLAGS) $(TSTFILES) -o $(TSTEXEC) $(INCDIR) $(LIBS)

lib:
	@$(MAKE) -C $(SRCDIR) $@ 

.PHONY: clean check

clean:
	@$(RM) -f *.o $(TSTEXEC)

check: build
	@echo "++++++NOW RUNNING THE TEST SUITE++++++";	\
	./$(TSTEXEC);					\
	echo "+++++DONE RUNNING THE TEST SUITE++++++";
