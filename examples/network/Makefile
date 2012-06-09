default: all

CC = ppc-amigaos-gcc
CFLAGS += -g
CFLAGS += -I../../include -L../../build
LIBS += -lgit2 -lpthread -lregex

OBJECTS = \
  git2.o \
  ls-remote.o \
  fetch.o \
  index-pack.o

all: $(OBJECTS)
	$(CC) $(CFLAGS) -o git2 $(OBJECTS) $(LIBS)
