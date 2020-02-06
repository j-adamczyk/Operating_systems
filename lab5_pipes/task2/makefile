COMMAND = gcc -Wall -std=gnu11

master:
	$(COMMAND) master.c -o master

slave:
	$(COMMAND) slave.c -o slave

main:
	$(COMMAND) main.c -o main

test: master slave main
	# 3 - number of slaves
	# 4 - lines to write per slave
	./main fifo_pipe 3 4 "./master" "./slave"
	make clean

clean:
	rm -f *.o main master slave fifo_pipe
