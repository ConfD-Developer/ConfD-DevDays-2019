######################################################################
usage:
	@echo "make all      Build all example files"
	@echo "make clean    Remove all built and intermediary files"
	@echo "make start    Start ConfD daemon"
	@echo "make stop     Stop any ConfD daemon and example agent"

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

CONFD_FLAGS = --addloadpath $(CONFD_DIR)/etc/confd --addloadpath ../fxs
START_FLAGS ?=

NODE 		 = 0
NNODES 		 = 3
NETCONF_SSH_BASE = 12022
NETCONF_BASE     = 12023
HTTP_BASE        = 8008
HTTPS_BASE       = 8888
IPC_BASE         = 4565

MAXNODE		= $(eval $NNODES - 1)

all:	router.fxs example-serial.fxs node$MAXNODE/confd.conf ssh-keydir
	@echo "Build complete"

%.fxs:  yang/%.yang
	$(CONFDC) -c --yangpath ./yang -o ./fxs/$@ -- $<

######################################################################
clean:	iclean
	-rm -rf node*
	-rm -rf *log cli-history 2> /dev/null || true

######################################################################
start:  stop
	@echo "### Start $(NNODES) confd daemons with our example specific confd-conf"
	@i=0; while [ $$i -lt $(NNODES) ]; do \
		echo "Starting node$$i" ; \
		( cd node$$i ; env sname=node$$i \
			 $(CONFD) -c confd.conf $(CONFD_FLAGS) ) ; \
		i=$$(($$i + 1)) ; \
	      done

node%/confd.conf:
	@i=0; \
	while [ $$i -lt $(NNODES) ]; do \
	   echo "Setting up node$$i" ; \
	   rm -rf node$$i ; \
	   mkdir -p node$$i/confd-cdb ;	\
	   cp $(CONFD_DIR)/var/confd/cdb/aaa_init.xml node$$i/confd-cdb ; \
	   sed -e "s|<!-- %SSH-PORT% -->|$$(($(NETCONF_SSH_BASE)+10*$$i))|" \
	       -e "s|<!-- %TCP-PORT% -->|$$(($(NETCONF_BASE)+10*$$i))|" \
	       -e "s|<!-- %HTTP-PORT% -->|$$(($(HTTP_BASE)+10*$$i))|" \
	       -e "s|<!-- %HTTPS-PORT% -->|$$(($(HTTPS_BASE)+10*$$i))|" \
	       -e "s|<!-- %IPC% -->|$$(($(IPC_BASE)+10*$$i))|" \
			< confd.conf.in > node$$i/confd.conf ; \
	   i=$$(($$i + 1)) ; \
	done

######################################################################
stop:
	@echo "###  Stop the confd daemons"
	@i=0; while [ $$i -lt $(NNODES) ]; do \
		echo "Stopping node$$i" ; \
		env CONFD_IPC_PORT=$$(($(IPC_BASE)+10*$$i)) $(CONFD) --stop ; \
		i=$$(($$i + 1)) ; \
	      done


######################################################################
cli:
	env CONFD_IPC_PORT=$$(($(IPC_BASE)+10*$(NODE))) \
		$(CONFD_DIR)/bin/confd_cli --user=admin --groups=admin \
			--interactive || echo Exit

cli1:
	@$(MAKE) NODE=1 cli

cli2:
	@$(MAKE) NODE=2 cli

cli1-c:
	@$(MAKE) NODE=1 cli-c

cli2-c:
	@$(MAKE) NODE=2 cli-c


cli-c:
	env CONFD_IPC_PORT=$$(($(IPC_BASE)+10*$(NODE))) \
		$(CONFD_DIR)/bin/confd_cli -C --user=admin --groups=admin \
			--interactive || echo Exit

######################################################################
query:
	$(CONFD_DIR)/bin/netconf-console -snoaaa --get-config --port=$$(($(NETCONF_SSH_BASE)+10*$(NODE)))

query1:
	@$(MAKE) NODE=1 query

query2:
	@$(MAKE) NODE=2 query

######################################################################
status:
	env CONFD_IPC_PORT=$$(($(IPC_BASE)+10*$(NODE))) $(CONFD) --status

status1:
	@$(MAKE) NODE=1 status

status2:
	@$(MAKE) NODE=2 status

######################################################################
