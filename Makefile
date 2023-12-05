CC      = g++
CFLAGS  = -g -std=c++2a

default: all

all: client server

client: client.cpp commands.h
	$(CC) $(CFLAGS) -o client client.cpp

server: server.cpp
	$(CC) $(CFLAGS) -o server server.cpp

clean:
	$(RM) client
