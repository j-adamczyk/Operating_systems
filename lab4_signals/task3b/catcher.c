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


int received_count = 0;
bool catching = true; // true - receiving, false - sending back
int mode; // -1: KILL, 0: SIGQUEUE, 1: SIGRT
int sender_PID = -1;


void react_to_normal_signal (int signal_number, siginfo_t *info, void *ucontext)
{
  received_count += 1;
  if (sender_PID == -1)
    sender_PID = info->si_pid;

  struct timespec time;
  time.tv_sec = 0;
  time.tv_nsec = 10;

  nanosleep (&time, NULL);

  kill (info->si_pid, SIGRTMIN);
}


void react_to_end_of_transmission (int signal_number, siginfo_t *info, void *ucontext)
{ catching = false; }


int main (int argc, char**argv)
{
  if (argc != 2)
  {
    perror ("Error: wrong number of arguments! There should be mode argument KILL, SIGQUEUE or SIGRT!");
    exit (1);
  }

  if (strcmp (argv[1], "KILL") == 0)
    mode = -1;
  else if (strcmp (argv[1], "SIGQUEUE") == 0)
    mode = 0;
  else if (strcmp (argv[1], "SIGRT") == 0)
    mode = 1;
  else
  {
    perror ("Error: second argument should be KILL, SIGQUEUE or SIGRT!");
    exit (1);
  }

  printf ("Catcher PID: %d\n", getpid());

  struct sigaction act;
  act.sa_flags = SA_SIGINFO;

  act.sa_sigaction = react_to_normal_signal;
  sigfillset (&act.sa_mask);
  sigdelset (&act.sa_mask, SIGUSR1);
  sigdelset (&act.sa_mask, SIGRTMIN+1);
  sigaction (SIGUSR1, &act, NULL);
  sigaction (SIGRTMIN+1, &act, NULL);

  act.sa_sigaction = react_to_end_of_transmission;
  sigfillset (&act.sa_mask);
  sigdelset (&act.sa_mask, SIGUSR2);
  sigdelset (&act.sa_mask, SIGRTMAX);
  sigaction (SIGUSR2, &act, NULL);
  sigaction (SIGRTMAX, &act, NULL);

  while (catching)
  {}

  int received_number = received_count;

  if (mode == -1) // KILL
  {
    while (received_count > 0)
    {
      if (kill (sender_PID, SIGUSR1) != 0)
        printf ("Warning: could not properly send back signal SIGUSR1.\n");
      received_count -= 1;
    }
    kill (sender_PID, SIGUSR2);
  }
  else if (mode == 0) // SIGQUEUE
  {
    union sigval value;
    value.sival_int = 0;
    while (received_count > 0)
    {
      if (sigqueue (sender_PID, SIGUSR1, value) != 0)
        printf ("Warning: could not properly send back signal SIGUSR1.\n");
      value.sival_int += 1;
      received_count -= 1;
    }
    sigqueue (sender_PID, SIGUSR2, value);
  }
  else // SIGRT
  {
    while (received_count > 0)
    {
      if (kill (sender_PID, SIGRTMIN+1) != 0) 
        printf ("Warning: could not properly send back signal SIGRTMIN.\n");
      received_count -= 1;
    }
    kill (sender_PID, SIGRTMAX);
  }

  printf ("Catcher report: %d signals caught and sent back. Ending catcher process.\n", received_number);

  return 0;
}
