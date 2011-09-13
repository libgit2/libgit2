.PHONY: all

CC = gcc
CFLAGS = -g -I../include
LFLAGS = -L../build -lgit2 -lz

all: general showindex

% : %.c
	$(CC) -o $@ $(CFLAGS) $< $(LFLAGS)

clean:
	$(RM) general showindex
