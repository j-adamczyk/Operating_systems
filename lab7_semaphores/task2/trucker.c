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

loader_info *loaders_info;

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


// ends trucker work
void sigint_handler (int signum, siginfo_t *info, void *ucontext)
{
  printf("\n%lld: SIGINT received by trucker, closing and removing conveyor belt.\n", get_time());

  munmap(packages, conveyor_belt->number_capacity * sizeof(package));
  shm_unlink(PACKAGES_SHM_NAME);

  munmap(conveyor_belt, sizeof(conveyer));
  shm_unlink(CONVEYOR_BELT_SHM_NAME);

  munmap(trucker_PID, sizeof(pid_t));
  shm_unlink(TRUCKER_PID_SHM_NAME);

  munmap(heaviest_weight, sizeof(int));
  shm_unlink(HEAVIEST_WEIGHT_SHM_NAME);
  
  sem_close(loader_semaphore);
  sem_unlink(LOADER_SEM_NAME);

  sem_close(trucker_semaphore);
  sem_unlink(TRUCKER_SEM_NAME);

  exit(0);
}


// new loader
void sigusr1_handler (int signal_number, siginfo_t *info, void *ucontext)
{
  printf("%lld: new loader with ID %d registered, he has packages with weight %d.\n", get_time(), info->si_pid, info->si_value.sival_int);
  loaders_info = add(loaders_info, info->si_pid, info->si_value.sival_int);
  if (info->si_value.sival_int > *heaviest_weight)
    *heaviest_weight = info->si_value.sival_int;
}


// loader ending work
void sigusr2_handler (int signal_number, siginfo_t *info, void *ucontext)
{
  loaders_info = rmv(loaders_info, info->si_pid);
  *heaviest_weight = get_highest_weight (loaders_info);
}


void move_conveyor_belt ()
{
  conveyor_belt->curr_num -= 1;
  conveyor_belt->curr_weight -= packages[0].weight;
  int i = 1;
  for (; i < conveyor_belt->number_capacity && packages[i-1].weight != 0; i++)
    packages[i-1] = packages[i];
  if (i < conveyor_belt->number_capacity)
    packages[i].weight = 0;
  else
    packages[conveyor_belt->number_capacity - 1].weight = 0;
}


int main (int argc, char **argv)
{
  if (argc != 4)
  {
    printf("Error: wrong number of arguments! There should be:\n- max weight of truck in weight units\n- max number of packages on conveyor belt\n- max weight units on conveyor belt\n");
    exit(1);
  }

  int truck_capacity = atoi(argv[1]);
  if (truck_capacity <= 0)
  {
    printf("Error: truck capacity must be a positive natural number!\n");
    exit(1);
  }

  int number_capacity = atoi(argv[2]);
  if (number_capacity <= 0)
  {
    printf("Error: conveyor belt packages number capacity must be a positive natural number!\n");
    exit(1);
  }

  int weight_capacity = atoi(argv[3]);
  if (weight_capacity <= 0)
  {
    printf("Error: conveyor belt weight capacity must be a positive natural number!\n");
    exit(1);
  }

  // signals to catch:
  // SIGINT - ends trucker
  // SIGUSR1 - informs about new loader
  // SIGUSR2 - informs about loader ending work
  struct sigaction act;
  act.sa_flags = SA_SIGINFO;

  act.sa_sigaction = sigint_handler;
  sigfillset (&act.sa_mask);
  sigdelset (&act.sa_mask, SIGINT);
  sigaction (SIGINT, &act, NULL);

  act.sa_sigaction = sigusr1_handler;
  sigfillset (&act.sa_mask);
  sigdelset (&act.sa_mask, SIGUSR1);
  sigaction (SIGUSR1, &act, NULL);

  act.sa_sigaction = sigusr2_handler;
  sigfillset (&act.sa_mask);
  sigdelset (&act.sa_mask, SIGUSR2);
  sigaction (SIGUSR2, &act, NULL);


  // initialize conveyor belt
  conveyor_belt_ID = shm_open(CONVEYOR_BELT_SHM_NAME, O_RDWR|O_CREAT, 0666);
  ftruncate(conveyor_belt_ID, sizeof(conveyer));
  conveyor_belt = mmap(NULL, sizeof(conveyer), PROT_READ|PROT_WRITE, MAP_SHARED, conveyor_belt_ID, 0);
  
  conveyor_belt->number_capacity = number_capacity;
  conveyor_belt->weight_capacity = weight_capacity;
  conveyor_belt->curr_num = 0;
  conveyor_belt->curr_weight = 0;

  // initialize packages array
  packages_ID = shm_open(PACKAGES_SHM_NAME, O_RDWR|O_CREAT, 0666);
  ftruncate(packages_ID, conveyor_belt->number_capacity * sizeof(package));
  packages = mmap(NULL, conveyor_belt->number_capacity * sizeof(package), PROT_READ|PROT_WRITE, MAP_SHARED, packages_ID, 0);
  for (int i = 0; i < number_capacity; i++)
  {
    package empty_package;
    empty_package.weight = 0;
    packages[i] = empty_package;
  }

  // initialize shared trucker PID
  trucker_PID_ID = shm_open(TRUCKER_PID_SHM_NAME, O_RDWR|O_CREAT, 0666);
  ftruncate(trucker_PID_ID, sizeof(int));
  trucker_PID = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED, trucker_PID_ID, 0);
  *trucker_PID = getpid();

  // initialize shared heaviest loader weight
  heaviest_weight_ID = shm_open(HEAVIEST_WEIGHT_SHM_NAME, O_RDWR|O_CREAT, 0666);
  ftruncate(heaviest_weight_ID, sizeof(int));
  heaviest_weight = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED, heaviest_weight_ID, 0);
  *heaviest_weight = 0;

  // initialize loader semaphore
  loader_semaphore = sem_open(LOADER_SEM_NAME, O_RDWR|O_CREAT, 0666, 1);

  // initialize trucker semaphore
  trucker_semaphore = sem_open(TRUCKER_SEM_NAME, O_RDWR|O_CREAT, 0666, 1);

  int curr_truck_weight = 0;
  int loader_semaphore_val;

  printf("%lld: empty truck ready for loading.\n", get_time());

  while(1)
  {
    // block trucker semaphore
    sem_wait(trucker_semaphore);

    // wait - it's possible that some loader is currently loading
    sem_getvalue(loader_semaphore, &loader_semaphore_val);
    if (loader_semaphore_val != 1)
      usleep(1e3);

    // load package if it's possible, otherwise get a new truck
    // check if package is available <=> first free place must not be first on conveyor belt
    if (conveyor_belt->curr_num > 0)
    {
      bool full = false;
      // new package is light enough to be loaded
      if (curr_truck_weight + packages[0].weight <= truck_capacity)
      {
        package curr_package = packages[0];
        curr_truck_weight += curr_package.weight;
        
        struct timeval curr_time;
        gettimeofday(&curr_time, NULL);
        long long int package_waiting_time = 1e6 * (curr_time.tv_sec - curr_package.load_time.tv_sec) + (curr_time.tv_usec - curr_package.load_time.tv_usec);

        move_conveyor_belt();

        printf("%lld: package from loader with ID %d loaded. It waited %lld us on conveyor belt. Truck now has load %d/%d.\n", get_time(), curr_package.loader_ID, package_waiting_time, curr_truck_weight, truck_capacity);

        // truck is full, it has to go
        if (curr_truck_weight == truck_capacity)
          full = true;
      }
      // new package is too heavy to be loaded
      else
        full = true;

      // truck is going, full or almost full
      if (full)
      {
        printf("%lld: truck full and ready to go with load %d/%d.\n", get_time(), curr_truck_weight, truck_capacity);
        curr_truck_weight = 0;
      }
    }
    // no packages available
    else
      printf("%lld: truck waiting for packages.\n", get_time());

    // unblock trucker semaphore
    sem_post(trucker_semaphore);
    usleep(1e5);
  }

  return 0;
}
