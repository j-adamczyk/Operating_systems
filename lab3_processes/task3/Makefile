COMMAND = gcc -Wall -std=gnu11 -g

all_monitor:
	$(COMMAND) -c monitor.c -o monitor.o
	$(COMMAND) monitor.o -o monitor

all_tester:
	$(COMMAND) -c tester.c -o tester.o
	$(COMMAND) tester.o -o tester

test_monitor_ram: all_monitor
	./monitor file_with_files 90 RAM 1 1

test_monitor_disc: all_monitor
	./monitor file_with_files 90 DISC 1 1

test_tester: all_tester
	./tester file1 1 2 256
	./tester file2 1 2 256
	make clean

# no make test, since program requires 2 terminals to concurrently run 2 processes
# use test_files and then test_monitor on one terminal and test_tester on the other

clean:
	rm -f file1 file2 monitor tester *.o main
