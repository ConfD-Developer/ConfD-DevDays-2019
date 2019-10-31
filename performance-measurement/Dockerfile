FROM debian:10-slim

ARG CONFD_VERSION
ARG PGW_VERSION
ARG NEX_VERSION
ARG PROM_VERSION
ARG GRFN_VERSION
ARG APP_NAME
ENV CONFD_VERSION=${CONFD_VERSION}
ENV PGW_VERSION=${PGW_VERSION}
ENV NEX_VERSION=${NEX_VERSION}
ENV PROM_VERSION=${PROM_VERSION}
ENV GRFN_VERSION=${GRFN_VERSION}
ENV APP_NAME=${APP_NAME}
ENV DEBIAN_FRONTEND=noninteractive
ENV CONFD_DIR=/confd
ENV USE_SSL_DIR=/usr/lib/x86_64-linux-gnu
ENV LD_LIBRARY_PATH=${USE_SSL_DIR}:$LD_LIBRARY_PATH
ENV PATH=${CONFD_DIR}/bin:${USE_SSL_DIR}/bin:/${APP_NAME}:$PATH
ENV CONFD=${CONFD_DIR}/bin/confd
ENV GF_SECURITY_ADMIN_USER=tailf
ENV GF_SECURITY_ADMIN_PASSWORD=tailf

WORKDIR /
RUN apt-get update \
    && apt-get install -y --no-install-recommends libxml2-utils python3 python3-pip python3-setuptools wget adduser libfontconfig1 build-essential libssl-dev curl openssh-client procps\
    && pip3 install --no-cache-dir paramiko

WORKDIR /tmp
RUN wget https://github.com/prometheus/pushgateway/releases/download/v${PGW_VERSION}/pushgateway-${PGW_VERSION}.linux-amd64.tar.gz \
    && tar xvzf /tmp/pushgateway-${PGW_VERSION}.linux-amd64.tar.gz -C / \
    && wget https://github.com/prometheus/node_exporter/releases/download/v${NEX_VERSION}/node_exporter-${NEX_VERSION}.linux-amd64.tar.gz \
    && tar xvfz node_exporter-${NEX_VERSION}.linux-amd64.tar.gz -C / \
    && wget https://github.com/prometheus/prometheus/releases/download/v${PROM_VERSION}/prometheus-${PROM_VERSION}.linux-amd64.tar.gz \
    && tar xvfz /tmp/prometheus-${PROM_VERSION}.linux-amd64.tar.gz -C / \
    && wget https://dl.grafana.com/oss/release/grafana-${GRFN_VERSION}.linux-amd64.tar.gz \
    && tar -zxvf grafana-${GRFN_VERSION}.linux-amd64.tar.gz -C /

COPY confd-${CONFD_VERSION}.linux.x86_64.installer.bin /tmp
WORKDIR ${CONFD_DIR}
RUN ln -s libcrypto.so /usr/lib/x86_64-linux-gnu/libcrypto.so.1.0.0 \
    && /tmp/confd-${CONFD_VERSION}.linux.x86_64.installer.bin ${CONFD_DIR}

# Add the ConfD cryptography integration and C-library API source
ADD confd-${CONFD_VERSION}.libconfd.tar.gz /tmp

# Rebuild the ConfD crypto integration and cleanup
WORKDIR /tmp/confd-${CONFD_VERSION}/libconfd
RUN make USE_SSL_DIR=${USE_SSL_DIR} crypto \
    && make install_crypto \
    && rm -rf /tmp/* /var/tmp/* \
    && apt-get autoremove -y \
    && apt-get clean \
    && ln -s /usr/bin/python3 /usr/bin/python

COPY confd-dashboard.yaml /grafana-${GRFN_VERSION}/conf/provisioning/dashboards/
COPY prometheus-datasource.yaml /grafana-${GRFN_VERSION}/conf/provisioning/datasources/

COPY pushgw-confd-*.sh /app/
ADD  ${APP_NAME}.tar.gz /app/
COPY prometheus.yml /app/

WORKDIR /app
EXPOSE 4565 2022 3000 9090
CMD [ "/app/pushgw-confd-testrun.sh" ]
