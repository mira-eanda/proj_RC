CC      = g++
CFLAGS  = -g -std=c++2a

default: all

all: client server

client: client.cpp commands.hpp common.hpp
	$(CC) $(CFLAGS) -o client client.cpp

server: server.cpp common.hpp
	$(CC) $(CFLAGS) -o server server.cpp

clean:
	$(RM) client
