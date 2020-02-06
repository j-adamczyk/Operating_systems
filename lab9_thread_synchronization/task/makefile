COMMAND = gcc -g -Wall -std=gnu11

all: main generate_filter

main:
	$(COMMAND) utils.c main.c -o main -lpthread

clean:
	rm -f *.o main
