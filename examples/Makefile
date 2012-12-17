.PHONY: all

CC = gcc
CFLAGS = -g -I../include -I../src -Wall -Wextra -Wmissing-prototypes -Wno-missing-field-initializers
LFLAGS = -L../build -lgit2 -lz
APPS = general showindex diff

all: $(APPS)

% : %.c
	$(CC) -o $@ $(CFLAGS) $< $(LFLAGS)

clean:
	$(RM) $(APPS)
	$(RM) -r *.dSYM
