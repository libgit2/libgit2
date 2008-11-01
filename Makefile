all::

DOXYGEN = doxygen

CFLAGS = -g -O2
OS     = unix

BASIC_CFLAGS := -Iinclude
BASIC_CFLAGS += -DGIT__PRIVATE
BASIC_CFLAGS += -fvisibility=hidden

OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))
HDRS = $(wildcard include/git/*.h)

OBJS += src/os/$(OS).o
HDRS += include/git/config.h
HDRS += include/git/os/$(OS).h


all:: libgit2.a

clean:
	rm -f libgit2.a src/*.o
	rm -f include/git/config.h
	rm -rf apidocs

apidocs:
	$(DOXYGEN) api.doxygen
	cp CONVENTIONS apidocs/

.c.o:
	$(CC) $(BASIC_CFLAGS) $(CFLAGS) -c $< -o $@

include/git/config.h: include/git/config.h.in
	sed 's/@@OS@@/$(OS)/g' $< >$@+
	mv $@+ $@

$(OBJS): $(HDRS)
libgit2.a: $(OBJS)
	rm -f libgit2.a
	$(AR) cr libgit2.a $(OBJS)

.PHONY: all
.PHONY: clean
.PHONY: apidocs
