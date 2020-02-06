#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wait.h>


int main (int argc, char **argv)
{
  if (argc != 3)
  {
    perror ("Error: wrong number of arguments! There should be fifo_file_name number_of_lines_to_write!");
    exit (1);
  }

  FILE *fifo_file = fopen (argv[1], "a");
  if (fifo_file == NULL)
  {
    perror ("Error: slave could not get access to FIFO file!");
    exit (1);
  }

  printf ("Slave with PID %d reporting.\n", getpid());

  srand (time (NULL) ^ getpid());

  int N = atoi (argv[2]);
  int counter = 0;
  while (counter < N)
  {
    char date[64];
    FILE *fp = popen ("date", "r");
    fgets (date, sizeof (date) - 1, fp);
    fprintf (fifo_file, "PID: %d, date: %s", getpid(), date);
    fflush (fifo_file);

    counter += 1;

    int wait = 1 + rand() % 5;
    sleep (wait);
  }

  return 0;
}
