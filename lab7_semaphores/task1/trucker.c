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

union semun
{
  int val;
  struct semid_ds* buf;
  unsigned short* array;
  struct seminfo* _buf;
};

loader_info *loaders_info;

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


// ends trucker work
void sigint_handler (int signum, siginfo_t *info, void *ucontext)
{
  printf("\n%lld: SIGINT received by trucker, closing and removing conveyor belt.\n", get_time());

  shmdt(conveyor_belt);
  shmctl(conveyor_belt_ID, IPC_RMID, NULL);

  shmdt(trucker_PID);
  shmctl(trucker_PID_ID, IPC_RMID, NULL);

  semctl(loader_semaphore_ID, 0, IPC_RMID);
  semctl(trucker_semaphore_ID, 0, IPC_RMID);

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
  conveyor_belt_key = ftok(FTOK_PATH, FTOK_CONVEYOR_BELT_SHM_SEED);
  conveyor_belt_ID = shmget(conveyor_belt_key, sizeof(conveyer), 0666|IPC_CREAT);
  conveyor_belt = shmat(conveyor_belt_ID, NULL, 0);
  
  conveyor_belt->number_capacity = number_capacity;
  conveyor_belt->weight_capacity = weight_capacity;
  conveyor_belt->curr_num = 0;
  conveyor_belt->curr_weight = 0;

  // initialize packages array
  packages_key = ftok(FTOK_PATH, FTOK_PACKAGES_SHM_SEED);
  packages_ID = shmget(packages_key, number_capacity * sizeof(package), 0666|IPC_CREAT);
  packages = (package *) shmat(packages_ID, NULL, 0);
  for (int i = 0; i < number_capacity; i++)
  {
    package empty_package;
    empty_package.weight = 0;
    packages[i] = empty_package;
  }

  // initialize loader semaphore
  loader_semaphore_key = ftok(FTOK_PATH, FTOK_LOADER_SEM_SEED);
  loader_semaphore_ID = semget(loader_semaphore_key, 1, 0666|IPC_CREAT);
  union semun arg;
  arg.val = 1;
  semctl(loader_semaphore_ID, 0, SETVAL, arg);

  // initialize trucker semaphore
  trucker_semaphore_key = ftok(FTOK_PATH, FTOK_TRUCKER_SEM_SEED);
  trucker_semaphore_ID = semget(trucker_semaphore_key, 1, 0666|IPC_CREAT);
  arg.val = 1;
  semctl(trucker_semaphore_ID, 0, SETVAL, arg);

  // initialize shared trucker PID
  trucker_PID_key = ftok(FTOK_PATH, FTOK_TRUCKER_PID_SHM_SEED);
  trucker_PID_ID = shmget(trucker_PID_key, sizeof(pid_t), 0666|IPC_CREAT);
  trucker_PID = shmat(trucker_PID_ID, NULL, 0);
  *trucker_PID = getpid();

  // initialize shared heaviest loader weight
  heaviest_weight_key = ftok(FTOK_PATH, FTOK_HEAVIEST_WEIGHT_SHM_SEED);
  heaviest_weight_ID = shmget(heaviest_weight_key, sizeof(int), 0666|IPC_CREAT);
  heaviest_weight = shmat(heaviest_weight_ID, NULL, 0);
  *heaviest_weight = 0;

  int curr_truck_weight = 0;
  printf("%lld: empty truck ready for loading.\n", get_time());

  struct sembuf sops[1];
  sops[0].sem_num = 0;
  while(1)
  {
    // block trucker semaphore
    sops[0].sem_op = -1;
    semop(trucker_semaphore_ID, sops, 1);

    // wait - it's possible that some loader is currently loading
    while (semctl(loader_semaphore_ID, 0, GETVAL) != 1)
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
    sops[0].sem_op = 1;
    semop(trucker_semaphore_ID, sops, 1);
    usleep(1e5);
  }

  return 0;
}
