all::

DOXYGEN = doxygen

CFLAGS = -g -O2 -Wall
OS     = unix

BASIC_CFLAGS := -Isrc
BASIC_CFLAGS += -fvisibility=hidden
ALL_CFLAGS = $(CFLAGS) $(BASIC_CFLAGS)

SRC_C = $(wildcard src/*.c)
OBJS = $(patsubst %.c,%.o,$(SRC_C))
HDRS = $(wildcard src/*.h)
PUBLIC_HEADERS = $(wildcard src/git/*.h)
HDRS += $(PUBLIC_HEADERS)
CONFIG_H = src/git/config.h

OBJS += src/os/$(OS).o
HDRS += src/git/config.h
HDRS += src/git/os/$(OS).h

GIT_LIB = libgit2.a

TEST_OBJ = $(patsubst %.c,%.o,\
           $(wildcard tests/t[0-9][0-9][0-9][0-9]-*.c))
TEST_EXE = $(patsubst %.o,%.exe,$(TEST_OBJ))
TEST_RUN = $(patsubst %.exe,%.run,$(TEST_EXE))

all:: $(GIT_LIB)

clean:
	rm -f $(GIT_LIB)
	rm -f src/*.o
	rm -f tests/*.o tests/*.exe tests/*.toc
	rm -f src/git/config.h
	rm -rf apidocs

apidocs:
	$(DOXYGEN) api.doxygen
	cp CONVENTIONS apidocs/

test: $(TEST_RUN)

sparse: $(CONFIG_H)
	sparse -DSPARSE_IS_RUNNING $(ALL_CFLAGS) $(SPARSE_FLAGS) $(SRC_C)

install-headers: $(PUBLIC_HEADERS)
	@mkdir -p /tmp/gitinc/git
	@for i in $^; do cat .HEADER $$i > /tmp/gitinc/$${i##src/}; done

.c.o:
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(CONFIG_H): $(CONFIG_H).in
	sed 's/@@OS@@/$(OS)/g' $< >$@+
	mv $@+ $@

$(OBJS): $(HDRS)
$(GIT_LIB): $(OBJS)
	rm -f $(LIB)
	$(AR) cr $(GIT_LIB) $(OBJS)

T_HDR         = tests/test_lib.h
T_LIB         = tests/test_lib.o
T_MAIN_C      = tests/test_main.c

$(T_LIB):    $(T_HDR) $(HDRS)
$(TEST_OBJ): $(T_HDR) $(HDRS)

$(patsubst %.exe,%.toc,$(TEST_EXE)): tests/%.toc: tests/%.c
	grep BEGIN_TEST $< >$@+
	mv $@+ $@

$(TEST_OBJ): tests/%.o: tests/%.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(patsubst %.exe,%_main.o,$(TEST_EXE)): tests/%_main.o: $(HDRS)
$(patsubst %.exe,%_main.o,$(TEST_EXE)): tests/%_main.o: $(T_MAIN_C)
$(patsubst %.exe,%_main.o,$(TEST_EXE)): tests/%_main.o: tests/%.toc
	$(CC) -Isrc -I. '-DTEST_TOC="$<"' \
		-c $(T_MAIN_C) \
		-o $@

$(TEST_EXE): tests/%.exe: $(T_LIB) $(GIT_LIB)
$(TEST_EXE): tests/%.exe: tests/%.o tests/%_main.o
	$(CC) -o $@ \
		$(patsubst %.exe,%_main.o,$@) \
		$(patsubst %.exe,%.o,$@) \
		$(T_LIB) -L. -lgit2

$(TEST_RUN): tests/%.run: tests/%.exe
	@$<

.PHONY: all
.PHONY: clean
.PHONY: test $(TEST_RUN)
.PHONY: apidocs
.PHONY: install-headers
.PHONY: sparse
