# NSO Interop example
#
# We use the standard ubuntu bas image.
FROM ubuntu:19.04
LABEL description="Docker image for NETCONF and YANG interop testing with NSO." maintainer="jojohans@cisco.com"

# Install the extra packages we need to run NSO, pioneer and DrNED
# Examiner. Only libssl is actually necessary for NSO itself, the
# python packages are for DrNED Examiner and DrNED.  The rest of the
# packages help us better understand what's going on inside the
# container and meassure network and other I/O performance.
RUN apt-get update && apt-get install -y \
        default-jre-headless \
        git \
        libssl-dev \
        make \
        openssh-client \
        python-lxml \
        python-paramiko \
        python-pexpect \
        python-pytest \
        libxml2-utils \
        xsltproc

# Install NSO

# What ncsrc usually does...
ENV NCS_DIR=/nso LD_LIBRARY_PATH=/nso/lib PATH=/nso/bin:$PATH PYTHONPATH=/nso/src/ncs/pyapi

## Install NSO in the container. And
COPY resources/nso-5.1.0.1.linux.x86_64.signed.bin /tmp
RUN (cd /tmp && ./nso-5.1.0.1.linux.x86_64.signed.bin)
RUN /tmp/nso-5.1.0.1.linux.x86_64.installer.bin $NCS_DIR
RUN ncs-setup --dest /nso/interop --no-netsim

# Install pioneer and drned-xmnr
RUN (cd nso/interop/packages && git clone https://github.com/NSO-developer/pioneer.git)
RUN (cd nso/interop/packages && git clone https://github.com/NSO-developer/drned-xmnr.git)
RUN (cd nso/interop/packages/pioneer/src && make clean all)
RUN (cd nso/interop/packages/drned-xmnr/src && make clean all)
RUN sed -i 's/  <load-path>/  <ncs-ipc-address>\n    <ip>0.0.0.0<\/ip>\n  <\/ncs-ipc-address>\n\n  <load-path>/' nso/interop/ncs.conf

# Enable verbose logging
COPY resources/init.xml /nso/interop/ncs-cdb

# Set working directory for NSO to the top directory of the mounted
# example.
WORKDIR /nso/interop

# Initially we only expose NETCONF (over ssh) and IPC ports.
# Uncomment to expose ports for other northbound protocols as
# necessary.
EXPOSE 2022 2023 2024 4565 8008 8088

# Cleanup
RUN apt-get autoremove -y && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Start init daemon and ConfD.
CMD ["/nso/bin/ncs", "--foreground", "-v", "--addloadpath", "/nso/interop"]
