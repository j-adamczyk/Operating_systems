#ifndef util_h
#define util_h

#define FTOK_PATH "/tmp"
#define FTOK_CONVEYOR_BELT_SHM_SEED 1
#define FTOK_LOADER_SEM_SEED 2
#define FTOK_TRUCKER_SEM_SEED 3
#define FTOK_TRUCKER_PID_SHM_SEED 4
#define FTOK_HEAVIEST_WEIGHT_SHM_SEED 5
#define FTOK_PACKAGES_SHM_SEED 6

typedef struct package
{
  pid_t loader_ID;
  int weight;
  struct timeval load_time;
} package;

typedef struct conveyer
{
  int number_capacity;
  int weight_capacity;

  int curr_num; // number of full slots -> slot with index curr_num is first free one
  int curr_weight;
} conveyer;

typedef struct loader_info
{
  pid_t loader_ID;
  int weight;
  struct loader_info *next;
} loader_info;

loader_info *add(loader_info *first, pid_t loader_ID, int weight);
loader_info *rmv(loader_info *first, pid_t loader_ID);
int get_highest_weight(loader_info *first);
long long int get_time();

#endif
