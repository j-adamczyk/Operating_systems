COMMAND = gcc -Wall -std=gnu11

main:
	$(COMMAND) main.c -o main

test: main
	./main commands_file
	make clean

clean:
	rm -f *.o main
