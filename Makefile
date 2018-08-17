CC=gcc
CFLAGS=-Wall `pkg-config fuse json-c --cflags --libs`
all: src/todofs.c
	$(CC) src/todofs.c -o bin/todofs $(CFLAGS)

