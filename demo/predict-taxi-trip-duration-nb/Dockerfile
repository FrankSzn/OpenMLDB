FROM openjdk:11.0.13-jre-slim-bullseye

LABEL org.opencontainers.image.source https://github.com/4paradigm/OpenMLDB

RUN  apt-get update \
     && apt-get install -y --no-install-recommends libgomp1 curl binutils procps python3 python3-pip python3-numpy \
     && rm -rf /var/lib/apt/lists/* \
     && pip install --no-cache-dir py4j==0.10.9 numpy lightgbm tornado requests pandas openmldb

COPY script /work/taxi-trip/

ENV LANG=en_US.UTF-8
ARG OPENMLDB_VERSION=0.3.0

COPY setup_openmldb.sh /
RUN /setup_openmldb.sh "${OPENMLDB_VERSION}"  && rm /setup_openmldb.sh

WORKDIR /work/taxi-trip
