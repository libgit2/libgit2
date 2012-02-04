.PHONY: all

CC = gcc
CFLAGS = -g -I../include -I../src
LFLAGS = -L../build -lgit2 -lz

all: general showindex diff

% : %.c
	$(CC) -o $@ $(CFLAGS) $< $(LFLAGS)

clean:
	$(RM) general showindex diff
	$(RM) -r *.dSYM
