COMMAND = gcc -g -Wall -std=gnu11

all: main generate_filter

main:
	$(COMMAND) main.c -o main -lpthread -lm

generate_filter:
	$(COMMAND) generate_filter.c -o generate_filter

test: main
	@for type in block interleaved; \
	do \
		./main 1 $$type test_image.pgm sharpen "result_1_$$type"; \
		./main 2 $$type test_image.pgm sharpen "result_2_$$type"; \
		./main 4 $$type test_image.pgm sharpen "result_4_$$type"; \
		./main 8 $$type test_image.pgm sharpen "result_8_$$type"; \
	done
	@make clean

clean:
	rm -f *.o main generate_filter
