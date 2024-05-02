CC = gcc
CFLAGS = -g -Wextra -Wall
SQLFLAG = -l sqlite3

server: server.c
	$(CC) $(CFLAGS) server.c $(SQLFLAG) -o server

build: server

run: server
	./server
