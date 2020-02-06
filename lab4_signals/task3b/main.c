#include <stdbool.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


int main (int argc, char **argv)
{
  if (argc != 3)
  {
    perror ("Error: wrong number of arguments! There should be: number_of_signals_to_send mode!");
    exit (1);
  }

  pid_t catcher_PID = 0;

  pid_t pid = vfork();
  if (pid == 0)
  {
    catcher_PID = getpid();
    execl ("./catcher", "./catcher", argv[2], NULL);
  }
  else
  {
    char pid_arg[8];
    sprintf (pid_arg, "%d", catcher_PID);

    pid = vfork();
    if (pid == 0)
    {
      execl ("./sender", "./sender", pid_arg, argv[1], argv[2], NULL);
    }
    else
    {
      sleep (1);
      exit(0);
    }
  }

  return 0;
}
