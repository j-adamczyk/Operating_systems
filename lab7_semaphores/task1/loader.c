#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

key_t conveyor_belt_key;
int conveyor_belt_ID;
conveyer *conveyor_belt;

key_t packages_key;
int packages_ID;
package *packages;

key_t trucker_PID_key;
int trucker_PID_ID;
pid_t *trucker_PID;

key_t heaviest_weight_key;
int heaviest_weight_ID;
int *heaviest_weight;

key_t loader_semaphore_key;
int loader_semaphore_ID;

key_t trucker_semaphore_key;
int trucker_semaphore_ID;

bool held_semaphore = false;

void sigint_handler(int signum)
{
  printf("\n%lld: SIGINT received by client, closing shared memory.\n", get_time());
  shmdt(conveyor_belt);
  if (held_semaphore)
  {
    struct sembuf sops[1];
    sops[0].sem_num = 0;
    sops[0].sem_op = 1;
    sops[0].sem_flg = 0;
    semop(loader_semaphore_ID, sops, 1);
  }
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

  conveyor_belt_key = ftok(FTOK_PATH, FTOK_CONVEYOR_BELT_SHM_SEED);
  conveyor_belt_ID = shmget(conveyor_belt_key, 0, 0666);
  conveyor_belt = shmat(conveyor_belt_ID, NULL, 0);

  packages_key = ftok(FTOK_PATH, FTOK_PACKAGES_SHM_SEED);
  packages_ID = shmget(packages_key, 0, 0666);
  packages = shmat(packages_ID, NULL, 0);

  trucker_PID_key = ftok(FTOK_PATH, FTOK_TRUCKER_PID_SHM_SEED);
  trucker_PID_ID = shmget(trucker_PID_key, 0, 0666);
  trucker_PID = shmat(trucker_PID_ID, NULL, 0);

  heaviest_weight_key = ftok(FTOK_PATH, FTOK_HEAVIEST_WEIGHT_SHM_SEED);
  heaviest_weight_ID = shmget(heaviest_weight_key, 0, 0666);
  heaviest_weight = shmat(heaviest_weight_ID, NULL, 0);

  loader_semaphore_key = ftok(FTOK_PATH, FTOK_LOADER_SEM_SEED);
  loader_semaphore_ID = semget(loader_semaphore_key, 0, 0666);

  trucker_semaphore_key = ftok(FTOK_PATH, FTOK_TRUCKER_SEM_SEED);
  trucker_semaphore_ID = semget(trucker_semaphore_key, 0, 0666);

  // add loader to list
  union sigval value;
  value.sival_int = package_weight;
  sigqueue(*trucker_PID, SIGUSR1, value);

  // used if SIGINT happens
  held_semaphore = true;

  struct sembuf sops[1];
  sops[0].sem_num = 0;
  while(packages_number != 0) // will never be achieved by endless loader
  {
    // check if trucker is not loading from conveyor belt
    if (semctl(trucker_semaphore_ID, 0, GETVAL) == 1)
    {
      // check if other loader is not loading on conveyor belt
      if (semctl(loader_semaphore_ID, 0, GETVAL) == 1)
      {
        // block loader semaphore
        sops[0].sem_op = -1;
        semop(loader_semaphore_ID, sops, 1);

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

          printf("%lld: oader with ID %d loaded package with weight %d.\n", get_time(), getpid(), package_weight);

          // for endless loader it'll be fine, since it'll never get to 0
          packages_number -= 1;
        }
        else
          printf("%lld: loader with ID %d waiting to be able to load package (there's too much packages).\n", get_time(), getpid());

        // unblock loader semaphore
        sops[0].sem_op = 1;
        semop(loader_semaphore_ID, sops, 1);
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
  sops[0].sem_op = -1;
  semop(loader_semaphore_ID, sops, 1);

  sigqueue(*trucker_PID, SIGUSR2, value);

  sops[0].sem_op = 1;
  semop(loader_semaphore_ID, sops, 1);

  shmdt(conveyor_belt);

  return 0;
}
