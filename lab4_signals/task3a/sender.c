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

volatile int received_count = 0;
volatile int should_have_received = 0;
bool catching = true;
int mode; // -1: KILL, 0: SIGQUEUE, 1: SIGRT


void react_to_normal_signal (int signal_number, siginfo_t *info, void *ucontext)
{
  received_count += 1;
  should_have_received = info->si_value.sival_int + 1;
}


void react_to_end_of_transmission (int signal_number, siginfo_t *info, void *ucontext)
{ catching = false; }


int main (int argc, char **argv)
{
  if (argc != 4)
  {
    perror ("Error: wrong number of arguments! There should be: catcher_PID number_of_signals_to_send mode!");
    exit (1);
  }

  int catcher_PID = atoi (argv[1]);
  int number_of_signals_to_send = atoi (argv[2]);

  if (strcmp (argv[3], "KILL") == 0)
  { mode = -1; }
  else if (strcmp (argv[3], "SIGQUEUE") == 0)
  { mode = 0; }
  else if (strcmp (argv[3], "SIGRT") == 0)
  { mode = 1; }
  else
  {
    perror ("Error: wrong mode! It should be KILL, SIGQUEUE or SIGRT!");
    exit (1);
  } 

  int sent = 0;
  if (mode == -1) // KILL
  {
    while (sent < number_of_signals_to_send)
    {
      if (kill (catcher_PID, SIGUSR1) != 0)
        printf ("Warning: could not properly send signal SIGUSR1.\n");
      sent += 1;
    }
    kill (catcher_PID, SIGUSR2);
  }
  else if (mode == 0) // SIGQUEUE
  {
    union sigval value;
    value.sival_int = 0;
    while (sent < number_of_signals_to_send)
    {
      if (sigqueue (catcher_PID, SIGUSR1, value) != 0)
        printf ("Warning: could not properly send signal SIGUSR1.\n");
      sent += 1;
    }
    sigqueue (catcher_PID, SIGUSR2, value);
  }
  else // SIGRT
  {
    while (sent < number_of_signals_to_send)
    {
      if (kill (catcher_PID, SIGRTMIN) != 0)
        printf ("Warning: could not properly send signal SIGRTMIN.\n");
      sent += 1;
    }
    kill (catcher_PID, SIGRTMAX);
  }

  struct sigaction act;
  act.sa_flags = SA_SIGINFO;

  act.sa_sigaction = react_to_normal_signal;
  sigfillset (&act.sa_mask);
  sigdelset (&act.sa_mask, SIGUSR1);
  sigdelset (&act.sa_mask, SIGRTMIN);
  sigaction (SIGUSR1, &act, NULL);
  sigaction (SIGRTMIN, &act, NULL);

  act.sa_sigaction = react_to_end_of_transmission;
  sigfillset (&act.sa_mask);
  sigdelset (&act.sa_mask, SIGUSR2);
  sigdelset (&act.sa_mask, SIGRTMAX);
  sigaction (SIGUSR2, &act, NULL);
  sigaction (SIGRTMAX, &act, NULL);

  while (catching)
  {}

  printf ("Sender reporting: %d signals received, should have received %d signals.\n", received_count, sent);

  return 0;
}
