COMMAND = gcc -g -Wall -std=gnu11

all: client server

client:
	$(COMMAND) utils.c client.c -o client -lpthread

server:
	$(COMMAND) utils.c server.c -o server -lpthread

rm_sock:
	rm sock

clean:
	rm -f *.o sock client server
