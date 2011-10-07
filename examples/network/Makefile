default: all

# If you've installed libgit2 to a non-standard location, you can use
# these lines to make pkg-config find it.

#LIBGIT2_PATH ?= $(HOME)/staging/libgit2/lib DEPS =
#$(shell PKG_CONFIG_PATH=$(LIBGIT2_PATH)/pkgconfig pkg-config --cflags
#--libs libgit2)

DEPS = $(shell pkg-config --cflags --libs libgit2)

CC = gcc
CFLAGS += -g
CFLAGS += $(DEPS)

OBJECTS = \
  git2.o \
  ls-remote.o \
  fetch.o \
  index-pack.o

all: $(OBJECTS)
	$(CC) $(CFLAGS) -o git2 $(OBJECTS)
