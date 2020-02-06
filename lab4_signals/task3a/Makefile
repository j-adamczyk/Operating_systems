COMMAND = gcc -Wall -std=gnu11 -g

catcher:
	$(COMMAND) -c catcher.c -o catcher.o
	$(COMMAND) catcher.o -o catcher

sender:
	$(COMMAND) -c sender.c -o sender.o
	$(COMMAND) sender.o -o sender

main:
	$(COMMAND) -c main.c -o main.o
	$(COMMAND) main.o -o main

all: catcher sender main

test_KILL: all
	./main 1000 KILL
	make clean

test_SIGQUEUE: all
	./main 1000 SIGQUEUE
	make clean

test_SIGRT: all
	./main 1000 SIGRT
	make clean

# no make test, since program requires 2 terminals to concurrently run 2 processes
# use test_files and then test_monitor on one terminal and test_tester on the other

clean:
	rm -f *.o sender catcher main
