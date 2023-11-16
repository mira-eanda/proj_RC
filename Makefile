CC      = gcc
CFLAGS  = -g
RM      = rm -f

default: all

all: client

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	$(RM) client
