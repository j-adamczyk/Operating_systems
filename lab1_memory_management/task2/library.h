#ifndef library
#define library


#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/stat.h>


typedef struct block_array
{
  size_t array_length;
  char **array;
  int free_index;
} block_array;


//block array utilities
block_array *create_block_array (size_t);
void delete_block (block_array*, int);
void delete_block_array (block_array*);
int get_free_index (block_array*, int);

//searching
int load_tmp_file (block_array*, char*);
void search (char*);

//setters
void set_curr_directory (char*);
void set_curr_file (char*);
void set_curr_search_pair (char*, char*);


#endif

