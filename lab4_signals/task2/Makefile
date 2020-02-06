COMMAND = gcc -Wall -std=gnu11 -g

monitor:
	$(COMMAND) -c monitor.c -o monitor.o
	$(COMMAND) monitor.o -o monitor

tester:
	$(COMMAND) -c tester.c -o tester.o
	$(COMMAND) tester.o -o tester

test_monitor: monitor
	./monitor file_with_files
	make clean

test_tester: tester
	./tester file1 3 5 8
	./tester file2 3 5 16
	make clean

# no make test, since program requires 2 terminals to concurrently run 2 processes
# use test_files and then test_monitor on one terminal and test_tester on the other

clean:
	rm -f file1 file2 monitor tester *.o main
