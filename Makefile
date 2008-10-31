all::

DOXYGEN = doxygen

CFLAGS = -g -O2
BASIC_CFLAGS = -Isrc

OBJS = \
	src/git_odb.o \
	src/git_oid.o \
#end OBJS

HDRS = \
	src/git_oid.h \
	src/git_common.h \
#end HDRS


all:: libgit2.a

clean:
	rm -f libgit2.a src/*.o
	rm -rf apidocs

apidocs:
	$(DOXYGEN) api.doxygen

.c.o:
	$(CC) $(BASIC_CFLAGS) $(CFLAGS) -c $< -o $@

src/%.o: src/%.c $(HDRS)
libgit2.a: $(OBJS)
	rm -f libgit2.a
	$(AR) cr libgit2.a $(OBJS)

.PHONY: all
.PHONY: clean
.PHONY: apidocs
