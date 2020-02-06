#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wait.h>


pid_t master_PID;
pid_t *slave_PIDs;
int number_of_slaves;


void wait_for_slaves()
{
  int status;
  for (int i=0; i<number_of_slaves; i++)
    waitpid (slave_PIDs[i], &status, 0);

  kill (SIGINT, master_PID);
}


int main (int argc, char **argv)
{
  if (argc != 6)
  {
    perror ("Error: wrong number of arguments! There should be fifo_file_name number_of_slaves number_of_lines_per_slave master_name slave_name!");
    exit (1);
  }

  char *fifo_file_name = argv[1];

  number_of_slaves = atoi (argv[2]);
  slave_PIDs = malloc (number_of_slaves * sizeof (pid_t));

  char *number_of_lines_per_slave = argv[3];
  char *master_name = argv[4];
  char *slave_name = argv[5];

  pid_t master = fork();

  if (master == 0)
  { execl (master_name, master_name, fifo_file_name, NULL); }
  else
  {
    master_PID = master;

    // 10 ns wait, since it's absolutely crucial that master creates file before slaves exist
    struct timespec time;
    time.tv_sec = 0;
    time.tv_nsec = 100;
    nanosleep (&time, NULL);
    
    for (int i=0; i<number_of_slaves; i++)
    {
      pid_t slave_PID = fork(); 

      if (slave_PID == 0)
      { execl (slave_name, slave_name, fifo_file_name, number_of_lines_per_slave, NULL); }
      else
      { slave_PIDs[i] = slave_PID; }
    }
  }

  wait_for_slaves();

  return 0;
}
