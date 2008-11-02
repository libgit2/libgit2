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

GIT_LIB = libgit2.a

TEST_OBJ = $(patsubst %.c,%.o,\
           $(wildcard tests/t[0-9][0-9][0-9][0-9]-*.c))
TEST_EXE = $(patsubst %.o,%.exe,$(TEST_OBJ))
TEST_RUN = $(patsubst %.exe,%.run,$(TEST_EXE))

all:: $(GIT_LIB)

clean:
	rm -f $(GIT_LIB)
	rm -f src/*.o
	rm -f tests/*.o tests/*.exe
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

T_HDR    = tests/test_lib.h
T_LIB    = tests/test_lib.o
T_MAIN_C  = tests/test_main.c
T_MAIN_O = tests/test_main.o

$(T_LIB): tests/test_lib.h $(HDRS)
$(TEST_EXE): $(T_LIB) $(T_HDR) $(T_MAIN_C) $(HDRS) $(GIT_LIB)

tests/%.exe: tests/%.o
	grep BEGIN_TEST $(patsubst %.o,%.c,$<) >tests/test_contents
	$(CC) $(CFLAGS) -Iinclude -c $(T_MAIN_C) -o $(T_MAIN_O)
	$(CC) -o $@ $(T_MAIN_O) $< $(T_LIB) -L. -lgit2
	rm -f $(T_MAIN_O) tests/test_contents

$(TEST_RUN): $(TEST_EXE)
	$<

.PHONY: all
.PHONY: clean
.PHONY: test $(TEST_RUN)
.PHONY: apidocs
