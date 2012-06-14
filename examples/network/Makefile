default: all

CC = gcc
CFLAGS += -g
CFLAGS += -I../../include -L../../build

OBJECTS = \
  git2.o \
  ls-remote.o \
  fetch.o \
  index-pack.o

all: $(OBJECTS)
	$(CC) $(CFLAGS) -o git2 $(OBJECTS)
