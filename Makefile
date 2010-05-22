
CCFLAGS=-std=c99 -Wall -Wextra -pedantic -D_GNU_SOURCE -g -ggdb
LDFLAGS=-lelf -ldl -Wl,-rpath,. -rdynamic
INCLUDES=-I .
CC=gcc

butcher: butcher.c bt.c bt.h  bt-private.h  common.h
		$(CC) $(CCFLAGS) $(LDFLAGS) $(INCLUDES) butcher.c bt.c -o butcher

bexec: bexec.c bt.h  bt-private.h  common.h
		$(CC) $(CCFLAGS) $(LDFLAGS) $(INCLUDES) bexec.c -o bexec

libfoo.so: libfoo.c bt.h
		$(CC) $(CCFLAGS) $(INCLUDES) -fPIC -shared libfoo.c -shared -o libfoo.so 

all: butcher bexec libfoo.so
