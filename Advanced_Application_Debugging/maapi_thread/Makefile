
######################################################################
# Interface Status example
# (C) 2008 Tail-f Systems
#
# See the README file for more information
######################################################################

usage:
	@echo "See README file for more instructions"
	@echo "make all     Build all example files"
	@echo "make clean   Remove all built and intermediary files"
	@echo "make start   Start CONFD daemon and example agent"
	@echo "make stop    Stop any CONFD daemon and example agent"
	@echo "make cli     Start the CONFD Command Line Interface"

######################################################################
# Where is ConfD installed? Make sure CONFD_DIR points it out
CONFD_DIR ?= ../../..

# Include standard ConfD build definitions and rules
include $(CONFD_DIR)/src/confd/build/include.mk

# In case CONFD_DIR is not set (correctly), this rule will trigger
$(CONFD_DIR)/src/confd/build/include.mk:
	@echo 'Where is ConfD installed? Set $$CONFD_DIR to point it out!'
	@echo ''

######################################################################
# Example specific definitions and rules
CDB_DIR1=cdb
CDB_DIR2=cdb
CONFD_FXS_DIR = $(CONFD_DIR)/etc/confd
CONFD_FLAGS = --addloadpath $(CONFD_FXS_DIR) 
START_FLAGS ?=
CONFD_HOST=127.0.0.1
# auto-compile all the .cli files
CCL=$(patsubst %.cli,%.ccl,$(wildcard *.cli))

CV=$(shell  echo "$(CONFDV)" | sed 's/\./''/g')


test:
	@echo Confd version is $(CV)

maapi_test: maapi_test.c
	@echo $(CFLAGS) $(LIBS)
	$(CC) -ggdb -o $@ $< $(CFLAGS) $(LIBS)

operC: oper

%.ccl: %.cli
	$(CONFDC) -c $<

yang:   example.fxs
	$(CONFDC) --emit-xsd example.xsd example.fxs
	$(CONFDC) --emit-xsd example.xsd example.fxs
	$(PYANG) --format yin example.yang > example.yin

yin:    
	-$(PYANG) --format yang example.yin > example2.yang

all:	example.fxs example.h maapi_test $(CCL) $(CDB_DIR2) ssh-keydir
	@echo "Build complete"
	cp aaa_init.xml $(CDB_DIR2); 
	cp 1000_obj.xml $(CDB_DIR1); 

$(CDB_DIR1):
	-mkdir -p $(CDB_DIR1) 2>/dev/null || true

$(CDB_DIR2):
	-mkdir -p $(CDB_DIR2) 2>/dev/null || true
example.fxs: ./example.yang 
	$(CONFDC) -c $< -o $@ 


######################################################################
clean:	iclean
	-rm -rf $(CS).cs *.fxs cdb/* logs/* *.xsd *.yin maapi_test prg_tr*

######################################################################

start:  stop
	$(CONFD) -c confd.conf $(CONFD_FLAGS)  # -w --ignore-initial-validation -i
startw:  stop
	$(CONFD) -c confd.conf $(CONFD_FLAGS) -w

start_i:  stop
	: ### Start the confd daemon with our example specific confd-config
	$(CONFD) -c confd.conf $(CONFD_FLAGS) -i

start2:  stop
	$(CONFD) -c ./confd.conf --start-phase0 $(CONFD_FLAGS); \
	$(CONFD) --wait-phase0; 
#	confd_load -l -m -F x conf2.xml 
	confd_cmd -c get_phase
	sleep 1; \
	confd_cmd -c dump_schema
	sleep 60;
	$(CONFD) --start-phase1 $(CONFD_FLAGS); \
	$(CONFD) --start-phase2 $(CONFD_FLAGS); 

######################################################################
stop:
	: ### Stopping any currently running confd daemon
	$(CONFD) --stop    || true

######################################################################
cli:
	$(CONFD_DIR)/bin/confd_cli --user=admin --groups=admin \
		--interactive || echo Exit
cli-c:
	$(CONFD_DIR)/bin/confd_cli --user=admin --groups=admin \
		--interactive --host $(CONFD_HOST) -C || echo Exit 
