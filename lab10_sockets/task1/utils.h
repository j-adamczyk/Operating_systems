#ifndef _UTILS_H
#define _UTILS_H

#include <sys/time.h>
#include <sys/types.h>
#include <stdint.h>

#define UNIX_PATH_MAX 108
#define MAX_CLIENTS 10

typedef enum sock_msg_type_t
{
  REGISTER,
  UNREGISTER,
  PING,
  PONG,
  OK,
  NAME_TAKEN,
  FULL,
  FAIL,
  WORK,
  WORK_DONE,
} sock_msg_type_t;

typedef struct sock_msg_t
{
  int type;
  int size;
  int name_size;
  int id;
  void *content;
  char *name;
} sock_msg_t;

typedef struct client_t
{
  int fd;
  char *name;
  int jobs;
  int inactive;
} client_t;

void print_errno(void);
void die_errno(void);
void die(char* msg);

#endif

