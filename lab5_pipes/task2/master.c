#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>


int main (int argc, char **argv)
{
  if (argc != 2)
  {
    perror ("Error: wrong number of arguments! There should be fifo_file_name!");
    exit (1);
  }

  mkfifo (argv[1], 0777);

  FILE *fifo_file = fopen (argv[1], "r+");
  if (fifo_file == NULL)
  {
    perror ("Error: could not properly create and open FIFO file!");
    exit (1);
  }

  size_t size = 64;
  char *line = malloc (size * sizeof (char));

  while (1)
  {
    while ((getline (&line, &size, fifo_file)) != -1)
      printf ("%s", line);
  }

  return 0;
}
