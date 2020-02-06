Command = gcc -Wall -std=gnu11

all:
	make static
	make shared
	make clean

static:
	$(Command) -c library.c -o libraryStatic.o
	ar rcs libraryStatic.a libraryStatic.o

shared:
	$(Command) -fPIC -c library.c -o libraryShared.o
	$(Command) -shared -fPIC -o libraryShared.so libraryShared.o

clean:
	rm -f *.o *.a *.so
