#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "utils.h"


int main (int argc, char **argv)
{
  if (argc != 2)
  {
    printf("Error: wrong number of arguments! You have to provide number of loaders to be spawned!\n");
    exit(1);
  }

  // checks if trucker exists through trying to use it's shared memory
  int conveyor_belt_ID = shm_open(CONVEYOR_BELT_SHM_NAME, O_RDONLY, 0666);
  if (conveyor_belt_ID == -1)
  {
    if (errno == ENOENT)
    {
      printf("Error: trucker has not yet been created! Create it first before spawning loaders!\n");
      exit(1);
    }
    else
    {
      printf("Error: unexpected error occured while trying to access conveyor belt memory.\n");
      exit(1);
    }
  }

  int number_of_loaders = atoi(argv[1]);
  for (int i = 0; i < number_of_loaders; i++)
  {
    srand(time(NULL));
    // mass of packages for loader is random and from 1 to 10
    int N = 1 + (rand() % 11);
    // 20% chance of spawning endless loader
    // 80% change of spawning loader with finite, random number of packages available (up to 10)
    pid_t pid = fork();
    if (pid == 0)
    {
      if (rand() % 100 < 20)
      {
        execl("/loader", "/loader", N, NULL);
      }
      else
      {
        int C = 1 + (rand() % 11);
        execl("/loader", "/loader", N, C, NULL);
      }
    }
  }

  return 0;
}
