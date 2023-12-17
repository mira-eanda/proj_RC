CC      = g++
CFLAGS  = -g -std=c++2a

default: all

all: user AS

user: client.cpp commands.hpp common.hpp
	$(CC) $(CFLAGS) -o user client.cpp

AS: server.cpp common.hpp database.hpp	server_commands.hpp
	$(CC) $(CFLAGS) -o AS server.cpp

clean:
	$(RM) user AS