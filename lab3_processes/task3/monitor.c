#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


void print_process_resource_usage()
{
  struct rusage child;
  struct rusage self;
  getrusage (RUSAGE_SELF, &self);
  getrusage (RUSAGE_CHILDREN, &child);

  double bilion = 1000000.0;

  printf("PID: %d\n", getpid());
  printf("User time self: %fs\n", self.ru_utime.tv_sec + (double) self.ru_utime.tv_usec/bilion);
  printf("System time self: %fs\n", self.ru_utime.tv_sec + (double) self.ru_utime.tv_usec/bilion);
  printf("User time children: %fs\n", child.ru_utime.tv_sec + (double) child.ru_utime.tv_usec/bilion);
  printf("System time children: %fs\n", child.ru_utime.tv_sec + (double) child.ru_utime.tv_usec/bilion);
  printf("Integral shared memory size self and children: %ldkB\n", self.ru_ixrss + child.ru_ixrss);
  printf("Integral unshared data size self and children: %ldkB\n", self.ru_idrss + child.ru_idrss);
  printf("Integral unshared stack size self and children: %ldkB\n", self.ru_isrss + child.ru_isrss);
}


int copy_file_to_archive (char *file_path, char* new_file, char *buffer, char *mode)
{
  // RAM - buffer contains file to be written, file_path doesn't matter
  if (strcmp (mode, "RAM") == 0)
  {
    FILE *file = fopen (new_file, "w");
    if (file == NULL)
    {
      char command[64];
      sprintf (command, "touch %s", new_file);
      file = fopen (new_file, "w");
      if (file == NULL)
        return -1;
    }

    int chars_written = fwrite (buffer, sizeof (char), strlen (buffer), file);
    if (chars_written != strlen (buffer))
      printf ("Warning: could not write all characters to file %s.\n", new_file);

    fclose (file);
  }
  // DISC - buffer doesn't matter, file at file_path should be copied
  else
  {
    pid_t child_PID = fork();

    if (child_PID < 0)
    {
      printf ("Warning: could not create copying process for file %s.\n", new_file);
      return -1;
    }
    else if (child_PID == 0)
    {
      int status = execlp ("cp", "cp", file_path, new_file, NULL);
      if (status == -1)
        return -1;
    }
    else
      wait (NULL);
  }

  return 0;
}


char *load_file (char *file_path)
{
  FILE *file = fopen (file_path, "r");

  struct stat file_info;
  lstat (file_path, &file_info);
  int file_length = file_info.st_size;

  char *loaded_file = calloc (file_length, sizeof (char));
  if (loaded_file == NULL)
  {
    perror("Error: did not have enough space for file!");
    exit(1);
  }

  fread (loaded_file, 1, file_length, file);

  return loaded_file;
}


int monitor_file (char *file_path, unsigned int monitoring_time, float interval, char *mode)
{
  int copies_created = 0;

  char *buffer;
  buffer = load_file (file_path);

  struct timespec start_time, end_time;
  clock_gettime (CLOCK_REALTIME, &start_time);
  clock_gettime (CLOCK_REALTIME, &end_time);

  struct stat file_info;
  lstat (file_path, &file_info);
  time_t prev_mod_time = file_info.st_mtime;
  time_t last_mod_time;

  while ((unsigned int) (end_time.tv_sec - start_time.tv_sec) < monitoring_time)
  {
    lstat (file_path, &file_info);
    last_mod_time = file_info.st_mtime;

    if (prev_mod_time < last_mod_time)
    {
      time_t current_time;
      time (&current_time);
      char time_buffer[32];
      strftime (time_buffer, 32, "_%Y-%m-%d_%H-%M-%S", localtime (&current_time));

      // new file name with directory, so file can be created directly there
      char new_file[256];
      sprintf (new_file, "archive/%s%s", basename (file_path), time_buffer);

      int copy_result = copy_file_to_archive (file_path, new_file, buffer, mode);
      if (copy_result == -1)
        printf ("Warning: could not create file %s.\n", new_file);

      if (strcmp (mode, "RAM") == 0)
        buffer = load_file (file_path);

      prev_mod_time = last_mod_time;
      copies_created += 1;
    }

    sleep (interval);
    clock_gettime (CLOCK_REALTIME, &end_time);
  }

  print_process_resource_usage();
  exit (copies_created);
}


void monitor (char *file_with_files_path, unsigned int monitoring_time, char *mode, unsigned int processor_time_limit_seconds, unsigned int virtual_memory_limit_MB)
{
  FILE *file = fopen (file_with_files_path, "r");

  if (file == NULL)
  {
    perror("Error: could not open provided file!");
    exit(1);
  }

  char line[512];
  int line_number = 0;
  while (fgets (line, sizeof (line), file) != NULL)
  {
    // only works if directories and file don't contain space (which is very common)
    char *file_to_monitor = strtok (line, " ");
    if (file_to_monitor == NULL)
    {
      printf ("Warning: could not properly read line %d.\n", line_number);
      line_number += 1;
      continue;
    }

    float interval = strtol(strtok(NULL, " "), NULL, 10);
    if (interval < 0)
    {
      printf ("Warning: wrong interval value at line %d.\n", line_number);
      line_number += 1;
      continue;
    }

    pid_t child_PID = fork();
    if (child_PID < 0)
    {
      printf ("Warning: could not create process for line %d.\n", line_number);
      line_number += 1;
      continue;
    }

    if (child_PID == 0)
    {
      if (file != NULL)
        fclose (file);

      struct rlimit new_limit;
      getrlimit (RLIMIT_CPU, &new_limit);

      new_limit.rlim_max = processor_time_limit_seconds;
      if (new_limit.rlim_cur > processor_time_limit_seconds)
        new_limit.rlim_cur = processor_time_limit_seconds;

      getrlimit (RLIMIT_AS, &new_limit);
      unsigned long int virtual_memory_limit_B = virtual_memory_limit_MB * 1e6;
      new_limit.rlim_max = virtual_memory_limit_B;
      if (new_limit.rlim_cur > virtual_memory_limit_B)
        new_limit.rlim_cur = virtual_memory_limit_B;

      if (strcmp (mode, "RAM") == 0)
      {
        if (monitor_file (file_to_monitor, monitoring_time, interval, "RAM") < 0)
          printf ("Warning: problem encountered in process for line %d.\n", line_number);
      }
      else
        if (monitor_file (file_to_monitor, monitoring_time, interval, "DISC") < 0)
          printf ("Warning: problem encountered in process for line %d.\n", line_number);
    }

    line_number += 1;
  }

  if (file != NULL)
  {
    fclose (file);
  }
}


void wait_for_monitoring_processes ()
{
  pid_t wpid;
  int status; // = copies created

  while ((wpid = wait (&status)) > 0)
  {
    if (WIFEXITED (status))
      printf ("Process with PID %d created %d copies of file.\n", wpid, WEXITSTATUS (status));
  }
}


int main (int argc, char **argv)
{
  if (argc != 6)
  {
    perror("Error: not enough arguments! There should be: file_with_files monitoring_time mode cpu_limit memory_limit");
    exit(1);
  }

  if (strtol (argv[2], NULL, 10) <= 0)
  {
    perror("Error: runtime in seconds was nonpositive!");
    exit(1);
  }

  if (strcmp (argv[3], "RAM") != 0 && strcmp (argv[3], "DISC") != 0)
  {
    perror("Error: mode not recognized!");
    exit(1);
  }

  if (strtol (argv[4], NULL, 10) <= 0)
  {
    perror("Error: CPU limit was nonpositive!");
    exit(1);
  }

  if (strtol (argv[5], NULL, 10) <= 0)
  {
    perror("Error: virtual memory limit was nonpositive!");
    exit(1);
  }

  monitor (argv[1], (unsigned int) strtol(argv[2], NULL, 10), argv[3], (unsigned int) strtol(argv[4], NULL, 10), (unsigned int) strtol(argv[5], NULL, 10));
  wait_for_monitoring_processes ();
  return 0;
}
