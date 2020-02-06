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
#include <unistd.h>

#include "utils.h"

int conveyor_belt_ID;
conveyer *conveyor_belt;

int packages_ID;
package *packages;

int trucker_PID_ID;
pid_t *trucker_PID;

int heaviest_weight_ID;
int *heaviest_weight;

sem_t *loader_semaphore;

sem_t *trucker_semaphore;

bool held_semaphore = false;

void sigint_handler(int signum)
{
  printf("\n%lld: SIGINT received by client, closing shared memory.\n", get_time());

  munmap(packages, conveyor_belt->number_capacity * sizeof(package));
  munmap(conveyor_belt, sizeof(conveyer));
  munmap(trucker_PID, sizeof(pid_t));
  munmap(heaviest_weight, sizeof(int));

  if (held_semaphore)
    sem_post(loader_semaphore);
  sem_close(loader_semaphore);
  sem_close(trucker_semaphore);

  exit(0);
}


int main (int argc, char **argv)
{
// N - masa paczek
  if (argc < 2)
  {
    printf("Error: wrong number of arguments! There has to be package weight for loader and possibly optional argument number of packages!\n");
    exit(1);
  }
  // opcjonalny argument - lista paczek

  int package_weight = atoi(argv[1]);
  if (package_weight <= 0)
  {
    printf("Error: package weight must be a positive natural number!\n");\
    exit(1);
  }

  int packages_number = -1;
  if (argc == 3)
  {
    packages_number = atoi(argv[2]);
    if (packages_number <= 0)
    {
      printf("Error: packages number optional argument, if provided, has to be a positive natural number!\n");
      exit(1);
    }
  }

  struct sigaction act;
  act.sa_handler = sigint_handler;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGINT);
  act.sa_flags = 0;
  sigaction(SIGINT, &act, NULL);

  conveyor_belt_ID = shm_open(CONVEYOR_BELT_SHM_NAME, O_RDWR, 0666);
  conveyor_belt = mmap(NULL, sizeof(conveyer), PROT_READ|PROT_WRITE, MAP_SHARED, conveyor_belt_ID, 0);

  packages_ID = shm_open(PACKAGES_SHM_NAME, O_RDWR, 0666);
  packages = mmap(NULL, conveyor_belt->number_capacity * sizeof(package), PROT_READ|PROT_WRITE, MAP_SHARED, packages_ID, 0);

  trucker_PID_ID = shm_open(TRUCKER_PID_SHM_NAME, O_RDONLY, 0666);
  trucker_PID = mmap(NULL, sizeof(pid_t), PROT_READ, MAP_SHARED, trucker_PID_ID, 0);

  heaviest_weight_ID = shm_open(HEAVIEST_WEIGHT_SHM_NAME, O_RDONLY, 0666);
  heaviest_weight = mmap(NULL, sizeof(int), PROT_READ, MAP_SHARED, heaviest_weight_ID, 0);

  loader_semaphore = sem_open(LOADER_SEM_NAME, O_RDWR);

  trucker_semaphore = sem_open(TRUCKER_SEM_NAME, O_RDONLY);

  // add loader to list
  union sigval value;
  value.sival_int = package_weight;
  sigqueue(*trucker_PID, SIGUSR1, value);

  // used if SIGINT happens
  held_semaphore = true;

  int trucker_semaphore_val;
  int loader_semaphore_val;
  while(packages_number != 0) // will never be achieved by endless loader
  {
    // check if trucker is not loading from conveyor belt
    sem_getvalue(trucker_semaphore, &trucker_semaphore_val);
    if (trucker_semaphore_val == 1)
    {
      sem_getvalue(loader_semaphore, &loader_semaphore_val);
      // check if other loader is not loading on conveyor belt
      if (loader_semaphore_val == 1)
      {
        // block loader semaphore
        sem_wait(loader_semaphore);

        // check if state of conveyor belt allows for package to be loaded
        if (conveyor_belt->curr_num + 1 <= conveyor_belt->number_capacity && conveyor_belt->curr_weight + package_weight <= conveyor_belt->weight_capacity && conveyor_belt->curr_weight + *heaviest_weight <= conveyor_belt->weight_capacity)
        {
          package new_package;
          new_package.loader_ID = getpid();
          new_package.weight = package_weight;
          gettimeofday(&new_package.load_time, NULL);

          packages[conveyor_belt->curr_num] = new_package;
          conveyor_belt->curr_num += 1;
          conveyor_belt->curr_weight += package_weight;

          printf("%lld: loader with ID %d loaded package with weight %d.\n", get_time(), getpid(), package_weight);

          // for endless loader it'll be fine, since it'll never get to 0
          packages_number -= 1;
        }
        else
          printf("%lld: loader with ID %d waiting to be able to load package (there's too much packages).\n", get_time(), getpid());

        // unblock loader semaphore
        sem_post(loader_semaphore);
        held_semaphore = false;
      }
      else
        printf("%lld: loader with ID %d waiting to be able to load package (another loader is loading).\n", get_time(), getpid());
    }
    else
      printf("%lld: loader with ID %d waiting to be able to load package (loader is loading package).\n", get_time(), getpid());
  }

  if (packages_number == 0)
    printf("%lld: loader with ID %d loaded all packages and is ending work.\n", get_time(), getpid());
  
  // remove loader from list, guarding it with semaphores
  sem_wait(loader_semaphore);

  sigqueue(*trucker_PID, SIGUSR2, value);

  sem_post(loader_semaphore);

  munmap(packages, conveyor_belt->number_capacity * sizeof(package));
  munmap(conveyor_belt, sizeof(conveyer));
  munmap(trucker_PID, sizeof(pid_t));
  munmap(heaviest_weight, sizeof(int));

  sem_close(loader_semaphore);
  sem_close(trucker_semaphore);

  return 0;
}
