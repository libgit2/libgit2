all::

DOXYGEN = doxygen

CFLAGS = -g -O2 -Wall
OS     = unix

BASIC_CFLAGS := -Iinclude
BASIC_CFLAGS += -DGIT__PRIVATE
BASIC_CFLAGS += -fvisibility=hidden

OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))
HDRS = $(wildcard include/git/*.h)

OBJS += src/os/$(OS).o
HDRS += include/git/config.h
HDRS += include/git/os/$(OS).h

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
	rm -f include/git/config.h
	rm -rf apidocs

apidocs:
	$(DOXYGEN) api.doxygen
	cp CONVENTIONS apidocs/

test: $(TEST_RUN)

.c.o:
	$(CC) $(BASIC_CFLAGS) $(CFLAGS) -c $< -o $@

include/git/config.h: include/git/config.h.in
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
	$(CC) -Iinclude $(CFLAGS) -c $< -o $@

$(patsubst %.exe,%_main.o,$(TEST_EXE)): tests/%_main.o: $(HDRS)
$(patsubst %.exe,%_main.o,$(TEST_EXE)): tests/%_main.o: $(T_MAIN_C)
$(patsubst %.exe,%_main.o,$(TEST_EXE)): tests/%_main.o: tests/%.toc
	$(CC) -Iinclude -I. '-DTEST_TOC="$<"' \
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
