.PHONY: all

CFLAGS = -g -I../include
LFLAGS = -Ldirectory-containing-libgit

all: general showindex

general : general.c
	gcc -o general $(CFLAGS) general.c $(LFLAGS) -lgit2 -lz

showindex : showindex.c
	gcc -o showindex $(CFLAGS) showindex.c $(LFLAGS) -lgit2 -lz

clean:
	rm general showindex
