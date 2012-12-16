default: all

CC = gcc
CFLAGS += -g
CFLAGS += -I../../include
LDFLAGS += -L../../build -L../.. -lgit2 -lpthread

OBJECTS = \
  git2.o \
  ls-remote.o \
  fetch.o \
  clone.o \
  index-pack.o

all: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o git2 $(OBJECTS)

clean:
	$(RM) $(OBJECTS)
	$(RM) git2
