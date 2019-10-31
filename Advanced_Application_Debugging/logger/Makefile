######################################################################
# Simple example
# (C) 2006 Tail-f Systems
#
# See the README file for more information
######################################################################

usage:
	@echo "See README file for more instructions"
	@echo "make all      Build all example files"
	@echo "make clean    Remove all built and intermediary files"
	@echo "make start    Start ConfD daemon and example agent"
	@echo "make stop     Stop any ConfD daemon and example agent"
	@echo "make query    Run NETCONF query against ConfD"
	@echo "make cli      Start the ConfD Command Line Interface"

######################################################################
# Where is ConfD installed? Make sure CONFD_DIR points it out
CONFD_DIR ?= ../../..

# Include standard ConfD build definitions and rules
include $(CONFD_DIR)/src/confd/build/include.mk

# In case CONFD_DIR is not set (correctly), this rule will trigger
$(CONFD_DIR)/src/confd/build/include.mk:
	@echo 'Where is ConfD installed? Set $$CONFD_DIR to point it out!'
	@echo ''
	@exit 1

######################################################################
# Example specific definitions and rules

CONFD_FLAGS = --addloadpath $(CONFD_DIR)/etc/confd 
START_FLAGS ?=

SRC	  = simple_withtrans.c
OBJS	  = $(SRC:.c=.o)

CCL=$(patsubst %.cli,%.ccl,$(wildcard *.cli))

%.ccl: %.cli
	$(CONFDC) -c $<

maapi: maapi_test

######################################################################
all:	smp.fxs smp.h simple_withtrans $(CDB_DIR) $(CCL) ssh-keydir
	@echo "Build complete"

simple_withtrans: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

validate: validate.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS)
######################################################################
clean:	iclean
	-rm -rf smp.h validate validate.dSYM simple_withtrans daemon.trace running.* logs/* 2> /dev/null || true

maapi_test: maapi_test.c
	$(CC) -ggdb -o $@ $< $(CFLAGS) $(LIBS)

start0:  stop
	$(CONFD) -c ./confd.conf $(CONFD_FLAGS) --start-phase0 

start:  stop
	$(CONFD) -c ./confd.conf $(CONFD_FLAGS)
	./simple_withtrans -t &
	sleep 2;
	confd_load -l -m -F x -f servers.xml 

start1:  stop
	$(CONFD) -c ./confd.conf $(CONFD_FLAGS)



startw:  stop
	$(CONFD) -c ./confd.conf $(CONFD_FLAGS) -w


######################################################################
stop:
	### Killing any confd daemon and simple_withtrans confd agents
	$(CONFD) --stop          || true
	$(KILLALL) simple_withtrans || true

######################################################################
cli:
	$(CONFD_DIR)/bin/confd_cli --user=admin --groups=admin \
		--interactive || echo Exit

######################################################################
query:
	$(CONFD_DIR)/bin/netconf-console-tcp cmd-get-servers.xml

######################################################################
