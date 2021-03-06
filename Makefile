CC = gcc
CFLAGS = -g -Wall

all: server agent

server: server.c
	$(CC) $(CFLAGS) server.c -o server

agent: agent.c
	$(CC) $(CFLAGS) agent.c -o agent