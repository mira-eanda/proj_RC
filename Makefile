CC      = g++
CFLAGS  = -g

default: all

all: client

client: client.cpp commands.h
	$(CC) $(CFLAGS) -o client client.cpp

clean:
	$(RM) client
