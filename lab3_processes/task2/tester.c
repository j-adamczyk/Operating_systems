#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


char *get_random_string (size_t length)
{
  char *result = calloc (sizeof (char), length+1);

  static char charset[] = "abcdefghijklmnopqrstuvwxyz";

  if (length)
  {
    for (int i=0; i<length; i++)
    {
      int sign = rand() % (int) (sizeof(charset) - 1); // -1, since rand is inclusive
      result[i] = charset[sign];
    }

    result[length] = '\0';
  }

  return result;
}


int main (int argc, char **argv)
{
  if (argc != 5)
  {
    perror ("Error: wrong argument number! There should be: file_name pmin pmax bytes.");
    return 1;
  }

  char *file_name = argv[1];

  int pmin = atoi (argv[2]);
  if (pmin < 0)
  {
    perror ("Error: pmin must be positive!");
    return 1;
  }

  int pmax = atoi (argv[3]);
  if (pmax < 0)
  {
    perror ("Error: pmax must be positive!");
    return 1;
  }

  size_t bytes = atoi (argv[4]);
  if (bytes < 0)
  {
    perror ("Error: bytes number must be positive!");
    return 1;
  }

  // generates random integer in range [0, pmax-pmin] and then adds pmin to get [pmin, pmax]
  srand (time (NULL));
  int frequency = rand() % (pmax - pmin);
  frequency += pmin;

  char *random_string;
  char buffer[256];
  char time_buffer[32];
  time_t current_time;

  for (int i=0; i<8; i++)
  {
    sleep (frequency);

    FILE *file = fopen (file_name, "a");
    if (file == NULL)
    {
      char command[64];
      sprintf (command, "touch %s", file_name);
      system (command);
      file = fopen (file_name, "a");
    }

    time (&current_time);
    strftime (time_buffer, 32, "_%Y-%m-%d_%H-%M-%S", localtime (&current_time));
    random_string = get_random_string (bytes);

    sprintf (buffer, "%d %d %s %s\n", getpid(), frequency, time_buffer, random_string);
    fwrite (buffer, sizeof (char), strlen (buffer), file);

    fclose (file);
  }


  free (random_string);
  return 0;
}
