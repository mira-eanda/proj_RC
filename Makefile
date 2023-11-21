CC      = g++
CFLAGS  = -g -std=c++2a

default: all

all: client

client: client.cpp commands.h
	$(CC) $(CFLAGS) -o client client.cpp

clean:
	$(RM) client
