COMMAND = gcc -Wall -std=gnu11

all:
	$(COMMAND) main.c -o main

test:
	make clean
	make all
	bash create_test_directories.sh
	./main test_directories "<" "22/03/19" "14:00:00"
	make clean

clean:
	rm -f *.o main; rm -rf test_directories
