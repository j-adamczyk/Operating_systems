COMMAND = gcc -Wall -std=gnu11

main:
	$(COMMAND) main.c -o main

test:
	make clean
	make main
	./main
	make clean

clean:
	rm -f *.o main
