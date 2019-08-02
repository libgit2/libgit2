ARG BASE
FROM $BASE
ARG CACHEBUST=1
RUN apt-get update
RUN apt-get -y install pkgconf clang git cmake curl libssl-dev libcurl4 libcurl4-openssl-dev libssh2-1-dev libz-dev valgrind openssh-client openssh-server
RUN if [ "$ARCH" != "armhf" -a "$ARCH" != "arm64" ]; then apt-get -y install openjdk-11-jre-headless; fi
RUN mkdir /var/run/sshd
