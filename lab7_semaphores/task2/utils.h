#ifndef util_h
#define util_h

const char *CONVEYOR_BELT_SHM_NAME;
const char *LOADER_SEM_NAME;
const char *TRUCKER_SEM_NAME;
const char *TRUCKER_PID_SHM_NAME;
const char *HEAVIEST_WEIGHT_SHM_NAME;
const char *PACKAGES_SHM_NAME;

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
