COMMAND = gcc -Wall -std=gnu11

all: server client

server:
	$(COMMAND) server.c -o server

client:
	$(COMMAND) client.c -o client -lrt

clean:
	rm -f *.o server client
