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

loader_info *add(loader_info *first, pid_t loader_ID, int weight)
{
  loader_info *new_loader = malloc(sizeof(loader_info));
  new_loader->loader_ID = loader_ID;
  new_loader->weight = weight;
  if (first == NULL)
    new_loader->next = NULL;
  else
    new_loader->next = first;
  return new_loader;
}


loader_info *rmv(loader_info *first, pid_t loader_ID)
{
  loader_info *curr_loader = first;

  if (curr_loader == NULL)
    return NULL;

  if (curr_loader->next == NULL)
  {
    if (curr_loader->loader_ID == loader_ID)
    {
      free(first);
      return NULL;
    }
    else
      return first;
  }

  loader_info *prev_loader = first;
  curr_loader = first->next;
  while (curr_loader != NULL)
  {
    if (curr_loader->loader_ID == loader_ID)
    {
      prev_loader->next = curr_loader->next;
      free(curr_loader);
      return first;
    }
    prev_loader = curr_loader;
    curr_loader = curr_loader->next;
  }

  return first;
}


int get_highest_weight(loader_info *first)
{
  if (first == NULL)
    return 0;

  int max_weight = 0;
  loader_info *curr_loader = first;
  while (curr_loader != NULL)
  {
    if (curr_loader->weight > max_weight)
      max_weight = curr_loader->weight;

    curr_loader = curr_loader->next;
  }

  return max_weight;
}


// utility function for calculating timestamp in microseconds
long long int get_time ()
{
  struct timeval curr_time;
  gettimeofday(&curr_time, NULL);
  long long int timestamp = 1e6 * curr_time.tv_sec + curr_time.tv_usec;
  return timestamp;
}
