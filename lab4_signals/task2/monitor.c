#include <stdbool.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


typedef struct monitoring_status
{
  pid_t pid;
  char *file_name;
  bool running;
} monitoring_status;


int running = 1;
static monitoring_status *monitoring_processes;
int number_of_processes;
volatile int copies_created = 0;


int copy_file_to_archive (char* new_file, char *buffer)
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

  return 0;
}


void start_PID (int i, pid_t pid)
{
  if (i == -1)
  {
    for (int i=0; i<number_of_processes; i++)
      if (monitoring_processes[i].pid == pid && monitoring_processes[i].pid != -1)
      {
        if (!monitoring_processes[i].running)
        {
          monitoring_processes[i].running = true;
          kill (monitoring_processes[i].pid, SIGUSR1);
        }
        break;
      }
  }
  else
  {
    if (monitoring_processes[i].pid == pid && monitoring_processes[i].pid != -1 && !monitoring_processes[i].running)
    {
      monitoring_processes[i].running = true;
      kill (monitoring_processes[i].pid, SIGUSR1);
    }
  }
}


void start_all ()
{
  for (int i=0; i<number_of_processes; i++)
    if (!monitoring_processes[i].running)
      start_PID (i, monitoring_processes[i].pid);
}


void stop_PID (int i, pid_t pid)
{
  if (i == -1)
  {
    for (int i=0; i<number_of_processes; i++)
      if (monitoring_processes[i].pid == pid && monitoring_processes[i].pid != -1)
      {
        if (monitoring_processes[i].running)
        {
          monitoring_processes[i].running = false;
          kill (monitoring_processes[i].pid, SIGUSR1);
        }
        break;
      }
  }
  else
  {
    if (monitoring_processes[i].pid == pid && monitoring_processes[i].pid != -1 && monitoring_processes[i].running)
    {
      monitoring_processes[i].running = false;
      kill (monitoring_processes[i].pid, SIGUSR1);
    }
  }
}


void stop_all ()
{
  for (int i=0; i<number_of_processes; i++)
    if (monitoring_processes[i].running)
      stop_PID (i, monitoring_processes[i].pid);
}


void switch_monitoring (int signum)
{
  if (running == 1)
    running = 0;
  else if (running == 0)
    running = 1;
}


void end_monitoring (int signum)
{
  running = -1;
}


void end()
{
  stop_all ();

  for (int i=0; i<number_of_processes; i++)
    kill (monitoring_processes[i].pid, SIGINT);

  pid_t wpid;
  int status; // = copies created

  while ((wpid = wait (&status)) > 0)
  {
    if (WIFEXITED (status))
      printf ("Process with PID %d created %d copies of file.\n", wpid, WEXITSTATUS(status));
  }
}


void list ()
{
  for (int i=0; i<number_of_processes; i++)
    if (monitoring_processes[i].running)
      printf ("Process %d is monitoring file %s.\n", monitoring_processes[i].pid, monitoring_processes[i].file_name);
}


char *load_file (char *file_path)
{
  FILE *file = fopen (file_path, "r");

  struct stat file_info;
  lstat (file_path, &file_info);
  int file_length = file_info.st_size;

  char *loaded_file = calloc (file_length, sizeof (char));

  fread (loaded_file, 1, file_length, file);

  return loaded_file;
}


int monitor_file (char *file_path, float interval)
{
  signal (SIGUSR1, switch_monitoring);
  signal (SIGINT, end_monitoring);

  char *buffer;
  buffer = load_file (file_path);

  struct timespec start_time, end_time;
  clock_gettime (CLOCK_REALTIME, &start_time);
  clock_gettime (CLOCK_REALTIME, &end_time);

  struct stat file_info;
  lstat (file_path, &file_info);
  time_t prev_mod_time = file_info.st_mtime;
  time_t last_mod_time;

  while (running != -1)
  {
    if (running == 1)
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

        int copy_result = copy_file_to_archive (new_file, buffer);
        if (copy_result == -1)
          printf ("Warning: could not create file %s.\n", new_file);

        buffer = load_file (file_path);

        prev_mod_time = last_mod_time;
        copies_created += 1;
      }

      sleep (interval);
      clock_gettime (CLOCK_REALTIME, &end_time);
    }
    else
      pause();
  }

  exit (copies_created);
}


void monitor (char *file_with_files_path)
{
  FILE *file = fopen (file_with_files_path, "r");

  if (file == NULL)
  {
    perror("Error: could not open provided file!");
    exit(1);
  }

  char line[512];
  number_of_processes = 0;
  while (fgets (line, sizeof(line), file) != NULL)
    number_of_processes += 1;

  rewind (file);

  monitoring_processes = malloc (number_of_processes * sizeof (monitoring_status));

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

    monitoring_status status;
    status.running = true;
    monitoring_processes[line_number] = status;

    pid_t child_PID = fork();
    if (child_PID < 0)
    {
      printf ("Warning: could not create process for line %d.\n", line_number);
      monitoring_processes[line_number].pid = -1;
      line_number += 1;
      continue;
    }
    else if (child_PID == 0)
    { monitor_file (file_to_monitor, interval); }
    else
    {
      monitoring_processes[line_number].pid = child_PID;
      monitoring_processes[line_number].file_name = (char *) malloc ((strlen (file_to_monitor) + 1) * sizeof (char));
      strcpy (monitoring_processes[line_number].file_name, file_to_monitor);
      line_number += 1;
    }
  }

  list ();

  if (file != NULL)
  {
    fclose (file);
  }
}


// char *file_with_files_path
int main (int argc, char **argv)
{
  if (argc != 2)
  {
    perror("Error: not enough arguments!");
    exit(1);
  }

  if (strcmp (argv[1], "") == 0)
  {
    perror("Error: file path cannot be empty!");
    exit(1);
  }

  mkdir ("archive", 0777);

  monitor (argv[1]);

  char command[128];
  while (true)
  {
    scanf ("%[^\n]%*c", command);

    if (strcmp (command, "LIST") == 0)
    { list(); }
    else if (strcmp (command, "START ALL") == 0)
    { start_all(); }
    else if (strcmp (command, "STOP ALL") == 0)
    { stop_all(); }
    else if (strcmp (command, "END") == 0)
    {
      end();
      return 0;
    }
    else if (strncmp (command, "START", 5) == 0)
    {
      char *word;
      word = strtok (command, " ");
      word = strtok (NULL, " ");
      pid_t pid = atoi (word);
      start_PID (-1, pid);
    }
    else if (strncmp (command, "STOP", 4) == 0)
    {
      char *word;
      word = strtok (command, " ");
      word = strtok (NULL, " ");
      pid_t pid = atoi (word);
      stop_PID (-1, pid);
    }
    else
    {
      printf ("Command not recognized!\n");
    }
  }

  return 0;
}
