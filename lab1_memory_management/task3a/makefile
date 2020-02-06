Command = gcc -Wall -std=gnu11
Output_file = results3a.txt
Arguments = a+ create_table 16 search_directory test_directories/small_directories file1 tmp load_file tmp search_directory test_directories/small_directories file2 tmp load_file tmp search_directory test_directories/medium_directories file1 tmp load_file tmp search_directory test_directories/medium_directories file2 tmp load_file tmp search_directory test_directories/large_directories file1 tmp load_file tmp search_directory test_directories/large_directories file2 tmp load_file tmp search_directory test_directories/large_directories file3 tmp load_file tmp search_directory test_directories/large_directories file4 tmp load_file tmp remove_block 0 remove_block 1 remove_block 2 remove_block 3 remove_block 4 remove_block 5 remove_block 6 load_file tmp remove_block 6 load_file tmp remove_block 6 load_file tmp remove_block 6 load_file tmp remove_block 6 remove_table

all:
	make static
	make shared
	make dynamic
	make clean

static:
	$(Command) -c library.c -o library.o
	$(Command) -c main.c -o main.o
	ar rcs library.a library.o
	$(Command) main.o library.a -o main

shared:
	$(Command) -c main.c -o main.o
	$(Command) -fPIC -c library.c -o libraryShared.o
	$(Command) -shared -fPIC -o ./libraryShared.so libraryShared.o
	$(Command) -o main main.o -L. ./libraryShared.so -I .

dynamic: shared
	$(Command) -c main.c -o main.o -D DYNAMIC="library.so"
	$(Command) -o main main.o -ldl -I .

create_test_directories:
	make clean
	bash create_test_directories.sh

test: create_test_directories
	@rm -f $(Output_file)
	@for type in static shared dynamic; \
	do \
		make clean ;\
		make $$type ;\
		echo "\n$$type \n" >> $(Output_file) ;\
		echo "\n$$type \n" ;\
		./main $(Output_file) $(Arguments) ;\
		echo "\n" ;\
	done
	@make clean

clean:
	rm -f *.o tmp error_log *.a *.so main; rm -rf test_directories

