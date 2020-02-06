COMMAND = gcc -g -Wall -std=gnu11

all: trucker loader loader_spawner

loader_spawner:
	$(COMMAND) loader_spawner.c -o loader_spawner -lrt -pthread

loader:
	$(COMMAND) utils.c loader.c -o loader -lrt -pthread

trucker:
	$(COMMAND) utils.c trucker.c -o trucker -lrt -pthread

clean:
	rm -f *.o loader_spawner loader trucker
