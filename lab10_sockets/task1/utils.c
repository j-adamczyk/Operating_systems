#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

void print_errno(void)
{
  fputs(strerror(errno), stderr);
}

void die_errno(void)
{
  fprintf(stderr, "%s\n", strerror(errno));
  exit(1);
}

void die(char* msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(1);
}
