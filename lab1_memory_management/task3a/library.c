#include "library.h"

static char *curr_directory = NULL;
static char *curr_file = NULL;


block_array *create_block_array (size_t array_length)
{
  if (array_length == 0)
  {
    printf("Error: array_length must be >0!aq");
    exit(1);
  }

  block_array *array = calloc (1,sizeof(block_array));
  array->array_length = array_length;

  char **blocks = (char**) calloc (array_length, sizeof(char*));
  array->array = blocks;

  array->free_index = 0;

  return array;
}

void delete_block (block_array *array, int index)
{
  if (array == NULL)
  {
    printf("Error: array pointer is NULL, it cannot be deleted!");
    exit(1);
  }
  if (index < 0 || index >= array->array_length)
  {
    printf("Error: index of array to delete from must be >=0 and <array_length!");
    exit(1);
  }

  free(array->array[index]);
  array->free_index = index;
}

void delete_block_array (block_array *array)
{
  if (array == NULL)
    return;

  for (int i = 0; i < array->array_length; i++)
    delete_block(array,i);
}

int get_free_index (block_array *array, int last_free_index)
{
  int i = last_free_index;
  while (array->array[i] != NULL && i < array->array_length - 1)
      i++;

  if (i == array->array_length - 1)
  {
    printf("Warning: no more free indices in the array, remove blocks before adding more.");
    i = -1;
  }

  return i;
}

int load_tmp_file (block_array *array, char *tmp_file_name)
{
  if (tmp_file_name == NULL)
  {
    printf("Error: temporary file name was NULL!");
    exit(1);
  }

  if (array->free_index == -1)
  {
    printf("Error: target array full, remove blocks or use other block array!");
    exit(1);
  }

  FILE *tmp_file = fopen (tmp_file_name,"r");
  
  struct stat file_info;
  stat (tmp_file_name,&file_info);
  int file_length = file_info.st_size;

  char *block = calloc (file_length, sizeof(char));
  fread (block,1,file_length,tmp_file);

  fclose (tmp_file);

  array->array[array->free_index] = block;
  int result = array->free_index;

  array->free_index = get_free_index (array,array->free_index);

  return result;
}

void search (char *tmp_file_name)
{
  if (curr_directory == NULL)
  {
    printf("Error: no current directory set, find cannot be performed!");
    exit(1);
  }

  if (curr_file == NULL)
  {
    printf("Error: no current file set, find cannot be performed!");
    exit(1);
  }

  if (tmp_file_name == NULL)
  {
    printf("Warning: no temporary file name provided, it was set to tmp_file.");
    tmp_file_name = "tmp_file";
  }

  char command[1024];
  sprintf(command,"find %s -name %s 1>%s 2>error_log",curr_directory,curr_file,tmp_file_name);
  system (command);
printf("\n%s %s\n", curr_directory, curr_file);
}

void set_curr_directory (char *directory)
{
  if (strlen(directory) == 0)
  { printf("Warning: directory name was empty, it should be changed before searching."); }

  if (curr_directory != NULL)
    free(curr_directory);

  int directory_length = strlen (directory);

  curr_directory = (char *) calloc (directory_length, sizeof(char));

  if (curr_directory != NULL)
    strcpy (curr_directory, directory);
}

void set_curr_file (char *file)
{
  if (strlen(file) == 0)
  { printf("Warning: file name was empty, it should be changed before searching."); }

  if (curr_file != NULL)
    free(curr_file);

  int file_length = strlen (file);

  curr_file = (char *) calloc (file_length, sizeof(char));

  if (curr_file != NULL)
    strcpy (curr_file, file);
}

void set_curr_search_pair (char *directory, char *file)
{
  if (strlen(directory) == 0)
  { printf("Warning: directory name was empty, it should be changed before searching."); }
  if (strlen(file) == 0)
  { printf("Warning: file name was empty, it should be changed before searching."); }

  if (curr_directory != NULL)
    free(curr_directory);
  if (curr_file != NULL)
    free(curr_file);

  int directory_length = strlen (directory);
  int file_length = strlen (file);

  curr_directory = (char *) calloc (directory_length, sizeof(char));
  curr_file = (char *) calloc (file_length, sizeof(char));

  if (curr_directory != NULL)
    strcpy (curr_directory, directory);

  if (curr_file != NULL)
    strcpy (curr_file, file);
}

