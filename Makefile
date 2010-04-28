all::

# Define NO_VISIBILITY if your compiler does not support symbol
# visibility in general (and the -fvisibility switch in particular).
#
# Define OPENSSL_SHA1 if you want use the SHA1 routines from the
# OpenSSL crypto library, rather than the built-in C versions.
#
# Define PPC_SHA1 if you want to use the bundled SHA1 routines that
# are optimized for PowerPC, rather than the built-in C versions.
#

DOXYGEN = doxygen
INSTALL = install
RANLIB  = ranlib
AR      = ar cr

prefix=/usr/local
libdir=$(prefix)/lib

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo no')

ifdef MSVC
	# avoid the MinGW and Cygwin configuration sections
	uname_S := Windows
endif

CFLAGS = -g -O2 -Wall
OS     = unix

EXTRA_SRC =
EXTRA_OBJ =
EXTRA_CFLAGS =

AR_OUT=
CC_OUT=-o # add a space

# Platform specific tweaks

ifeq ($(uname_S),Windows)
	OS=win32
	RANLIB = echo
	CC = cl -nologo
	AR = lib -nologo
	CFLAGS = -TC -W3 -RTC1 -Zi -DWIN32 -D_DEBUG -D_LIB
	AR_OUT=-out:
	CC_OUT=-Fo
	NO_VISIBILITY=YesPlease
endif

ifneq (,$(findstring CYGWIN,$(uname_S)))
	NO_VISIBILITY=YesPlease
endif

ifneq (,$(findstring MINGW,$(uname_S)))
	OS=win32
	NO_VISIBILITY=YesPlease
	SPARSE_FLAGS=-Wno-one-bit-signed-bitfield
endif

-include config.mak

SRC_C = $(wildcard src/*.c)
OS_SRC = $(wildcard src/$(OS)/*.c)
SRC_C += $(OS_SRC) $(EXTRA_SRC)
OBJS = $(patsubst %.c,%.o,$(SRC_C)) $(EXTRA_OBJ)
HDRS = $(wildcard src/*.h)
PUBLIC_HEADERS = $(wildcard src/git/*.h)
HDRS += $(PUBLIC_HEADERS)

GIT_LIB = libgit2.a

TEST_OBJ = $(patsubst %.c,%.o,\
           $(wildcard tests/t[0-9][0-9][0-9][0-9]-*.c))
TEST_EXE = $(patsubst %.o,%.exe,$(TEST_OBJ))
TEST_RUN = $(patsubst %.exe,%.run,$(TEST_EXE))
TEST_VAL = $(patsubst %.exe,%.val,$(TEST_EXE))

ifdef PPC_SHA1
	EXTRA_SRC += src/ppc/sha1.c
	EXTRA_OBJ += src/ppc/sha1ppc.o
	EXTRA_CFLAGS += -DPPC_SHA1
else
ifdef OPENSSL_SHA1
	EXTRA_CFLAGS += -DOPENSSL_SHA1
else
	EXTRA_SRC += src/block-sha1/sha1.c
endif
endif

BASIC_CFLAGS := -Isrc
ifndef NO_VISIBILITY
BASIC_CFLAGS += -fvisibility=hidden
endif

ALL_CFLAGS = $(CFLAGS) $(BASIC_CFLAGS) $(EXTRA_CFLAGS)

all:: $(GIT_LIB)

clean:
	rm -f $(GIT_LIB)
	rm -f libgit2.pc
	rm -f *.pdb
	rm -f src/*.o src/*/*.o
	rm -rf apidocs
	rm -f *~ src/*~ src/*/*~
	@$(MAKE) -C tests -s --no-print-directory clean
	@$(MAKE) -s --no-print-directory cov-clean

test-clean:
	@$(MAKE) -C tests -s --no-print-directory clean

apidocs:
	$(DOXYGEN) api.doxygen
	cp CONVENTIONS apidocs/

test: $(GIT_LIB)
	@$(MAKE) -C tests --no-print-directory test

valgrind: $(GIT_LIB)
	@$(MAKE) -C tests --no-print-directory valgrind

sparse:
	cgcc -no-compile $(ALL_CFLAGS) $(SPARSE_FLAGS) $(SRC_C)

install-headers: $(PUBLIC_HEADERS)
	@$(INSTALL) -d /tmp/gitinc/git
	@for i in $^; do cat .HEADER $$i > /tmp/gitinc/$${i##src/}; done

install: $(GIT_LIB) $(PUBLIC_HEADERS) libgit2.pc
	@$(INSTALL) -d $(DESTDIR)/$(prefix)/include/git
	@for i in $(PUBLIC_HEADERS); do \
		cat .HEADER $$i > $(DESTDIR)/$(prefix)/include/$${i##src/}; \
	done
	@$(INSTALL) -d $(DESTDIR)/$(libdir)
	@$(INSTALL) $(GIT_LIB) $(DESTDIR)/$(libdir)/libgit2.a
	@$(INSTALL) -d $(DESTDIR)/$(libdir)/pkgconfig
	@$(INSTALL) libgit2.pc $(DESTDIR)/$(libdir)/pkgconfig/libgit2.pc

uninstall:
	@rm -f $(DESTDIR)/$(libdir)/libgit2.a
	@rm -f $(DESTDIR)/$(libdir)/pkgconfig/libgit2.pc
	@for i in $(PUBLIC_HEADERS); do \
		rm -f $(DESTDIR)/$(prefix)/include/$${i##src/}; \
	done
	@rmdir $(DESTDIR)/$(prefix)/include/git

.c.o:
	$(CC) $(ALL_CFLAGS) -c $< $(CC_OUT)$@

.S.o:
	$(CC) $(ALL_CFLAGS) -c $< $(CC_OUT)$@

$(OBJS): $(HDRS)
$(GIT_LIB): $(OBJS)
	rm -f $(GIT_LIB)
	$(AR) $(AR_OUT)$(GIT_LIB) $(OBJS)
	$(RANLIB) $(GIT_LIB)

$(TEST_OBJ) $(TEST_EXE) $(TEST_RUN) $(TEST_VAL): $(GIT_LIB)
	@$(MAKE) -C tests --no-print-directory \
		OS=$(OS) $(@F)

libgit2.pc: libgit2.pc.in
	sed -e 's#@prefix@#$(prefix)#' -e 's#@libdir@#$(libdir)#' $< > $@

.PHONY: all
.PHONY: clean
.PHONY: test $(TEST_VAL) $(TEST_RUN) $(TEST_EXE) $(TEST_OBJ)
.PHONY: apidocs
.PHONY: install-headers
.PHONY: install uninstall
.PHONY: sparse

### Test suite coverage testing
#
.PHONY: coverage cov-clean cov-build cov-report

COV_GCDA = $(patsubst %.c,%.gcda,$(SRC_C))
COV_GCNO = $(patsubst %.c,%.gcno,$(SRC_C))

coverage:
	@$(MAKE) -s --no-print-directory clean
	@$(MAKE) --no-print-directory cov-build
	@$(MAKE) --no-print-directory cov-report

cov-clean:
	rm -f $(COV_GCDA) $(COV_GCNO) *.gcov untested

COV_CFLAGS = $(CFLAGS) -O0 -ftest-coverage -fprofile-arcs

cov-build:
	$(MAKE) CFLAGS="$(COV_CFLAGS)" all
	$(MAKE) TEST_COVERAGE=1 test

cov-report:
	@echo "--- untested files:" | tee untested
	@{ for f in $(SRC_C); do \
		gcda=$$(dirname $$f)"/"$$(basename $$f .c)".gcda"; \
		if test -f $$gcda; then \
			gcov -b -p -o $$(dirname $$f) $$f >/dev/null; \
		else \
			echo $$f | tee -a untested; \
		fi; \
	   done; }
	@rm -f *.h.gcov
	@echo "--- untested functions:" | tee -a untested
	@grep '^function .* called 0' *.c.gcov \
	| sed -e 's/\([^:]*\)\.gcov:function \([^ ]*\) called.*/\1: \2/' \
	| sed -e 's|#|/|g' | tee -a untested

