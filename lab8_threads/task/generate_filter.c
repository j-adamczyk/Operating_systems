#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>


void generate_filter (char *filter_file_name, int size)
{
  srand(time(NULL));

  FILE *file = fopen(filter_file_name, "w+");
  if (errno != 0)
  {
    perror("Error");
    exit(1);
  }

  // first line contains filter size
  char filter_size[10];
  sprintf(filter_size, "%d\n", size);
  fwrite(filter_size, sizeof(char), strlen(filter_size), file);

  // next lines are actual filter
  float sum = 1; // sum of filter elements should equal 1
  float single_number_limit = sum / (size*size / 2);
  char *line = malloc(33 * size + 3);
  char number[32];
  float random_number;
  for (int i = 0; i < size; i++)
  {
    memset(line, 0, strlen(line));
    for (int j = 0; j < size; j++)
    {
      memset(number, 0, strlen(number));
      if (j == size - 1 && i == size - 1)
        random_number = sum;
      else
        random_number = (float) rand() / (float) (RAND_MAX / single_number_limit);
      sum -= random_number;
      sprintf(number, "%f ", random_number);
      line = strcat(line, number);
    }
    line = strcat(line, "\n");
    fwrite(line, sizeof(char), strlen(line), file);
  }

  fclose(file);
}


int main (int argc, char **argv)
{
  // process arguments
  if (argc != 3)
  {
    printf("Error: wrong number of arguments! File name and size of filter must be provided!\n");
    exit(1);
  }

  char *filter_file_name = argv[1];

  int size = atoi(argv[2]);
  if (size <= 0)
  {
    printf("Error: size must be a positive natural number!\n");
    exit(1);
  }

  // generate filter with sum of elements = 1
  generate_filter(filter_file_name, size);

  return 0;
}
