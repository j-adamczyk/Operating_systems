Command = gcc -Wall -std=gnu11
Output_file = raport2.txt
Arguments = w create_table 16 search_directory test_directories/small_directories file1 tmp load_file tmp search_directory test_directories/small_directories file2 tmp load_file tmp search_directory test_directories/medium_directories file1 tmp load_file tmp search_directory test_directories/medium_directories file2 tmp load_file tmp search_directory test_directories/large_directories file1 tmp load_file tmp search_directory test_directories/large_directories file2 tmp load_file tmp search_directory test_directories/large_directories file3 tmp load_file tmp search_directory test_directories/large_directories file4 tmp load_file tmp remove_block 0 remove_block 1 remove_block 2 remove_block 3 remove_block 4 remove_block 5 remove_block 6 load_file tmp remove_block 6 load_file tmp remove_block 6 load_file tmp remove_block 6 load_file tmp remove_block 6 remove_table

static:
	$(Command) -c library.c -o libraryStatic.o
	ar rcs libraryStatic.a libraryStatic.o
	$(Command) -c main.c -o main.o
	$(Command) main.o libraryStatic.a -o main

create_test_directories:
	make clean
	bash create_test_directories.sh

test: create_test_directories static
	./main $(Output_file) $(Arguments)
	make clean

clean:
	rm -f *.o tmp error_log *.a *.so main; rm -rf test_directories

