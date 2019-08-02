ARG BASE
FROM $BASE
ARG CACHEBUST=1
RUN apt-get update
RUN apt-get -y install pkgconf clang git cmake curl libssl-dev libcurl3 libcurl3-gnutls libcurl4-gnutls-dev valgrind openssh-client openssh-server openjdk-8-jre
RUN mkdir /var/run/sshd
