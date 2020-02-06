#define _XOPEN_SOURCE
#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wait.h>


volatile bool running = true;
int child_PID = -1;


void react_to_SIGINT (int signal_number)
{
  printf ("\nReceived termination signal.\n");
  exit(0);
}


void react_to_SIGTSTP (int signal_number)
{
  if (running)
  {
    printf ("\nWaiting for CTRL+Z (continuation) or CTRL+C (termination).\n");
    running = !running;
  }
  else
  {
    printf("\n");
    running = !running;
  }
}


void react_to_SIGINT_child (int signal_number)
{
  kill (child_PID, SIGKILL);
  printf ("\nReceived termination signal.\n");
  exit(0);
}


void react_to_SIGTSTP_child (int signal_number)
{
  if (running)
  {
    printf ("\nWaiting for CTRL+Z (continuation) or CTRL+C (termination).\n");
    running = !running;
    kill (child_PID, SIGKILL);
    child_PID = -1;
  }
  else
    running = !running;
}


int print_date_with_shell (char *shell_script)
{
  int status = execlp ("bash", "bash", shell_script, NULL);
  if (status == -1)
    return 1;
  else
    return 0;
}


int main (int argc, char **argv)
{
  if (argc == 1)
  {
    signal (SIGTSTP, react_to_SIGTSTP);

    struct sigaction act;
    act.sa_handler = react_to_SIGINT;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction (SIGINT, &act, NULL);

    while (true)
    {
      if (running)
      {
        time_t current_time;
        time (&current_time);
        char time_buffer[32];
        strftime (time_buffer, 32, "_%Y-%m-%d_%H-%M-%S", localtime (&current_time));
        printf ("%s\n", time_buffer);
        sleep (1);
      }
      else
        pause ();
    }
  }
  else if (argc == 2)
  {
    signal (SIGTSTP, react_to_SIGTSTP);

    struct sigaction act;
    act.sa_handler = react_to_SIGINT;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction (SIGINT, &act, NULL);

    while (true)
    {
      if (running)
      {
        if (child_PID == -1)
        {
          int print_result = print_date_with_shell (argv[1]);
          if (print_result == 1)
          {
            perror ("Error: could not use shell script!");
            exit (1);
          }
        }
      }
      else
      {
        pause ();
      }
    }

  }
  else
  {
    perror ("Error: wrong number of arguments!");
    exit (1);
  }

  return 0;
}
