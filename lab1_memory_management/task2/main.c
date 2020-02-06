#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/times.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


#include "library.h"

int is_a_number (const char *s)
{
    if (s == NULL || *s == '\0' || isspace(*s))
      return 0;
    char *p = NULL;
    strtod (s, &p);
    return *p == '\0';
}

void write_times (char *thing_measured, struct timespec *real_start_time, struct timespec *real_end_time, struct tms *start_times, struct tms *end_times, FILE *output_file)
{
  //1e9 to get time in nanoseconds
  long long int real_time = 1e9 * (real_end_time->tv_sec - real_start_time->tv_sec);
  real_time += (long long int) (real_end_time->tv_nsec - real_start_time->tv_nsec);

  long long int user_time = 1e9 * (end_times->tms_utime - start_times->tms_utime) / sysconf(_SC_CLK_TCK);
  user_time += 1e9 * (end_times->tms_cutime - start_times->tms_cutime) / sysconf(_SC_CLK_TCK);

  long long int system_time = 1e9 * (end_times->tms_stime - start_times->tms_stime) / sysconf(_SC_CLK_TCK);
  system_time += 1e9 * (end_times->tms_cstime - start_times->tms_cstime) / sysconf(_SC_CLK_TCK);

  char times[128];

  sprintf(times, strcat(thing_measured, "Real time: %Ld ns, user time: %Ld ns, system time: %Ld ns\n"), real_time, user_time, system_time);
  printf("%s \n", times);

  fprintf(output_file, "%s \n", times);
}

int main (int argc, char **argv)
{
  if (argc <= 4)
  {
    printf("Error: not enough arguments passed to program, minimum 4 are required!");
    exit(1);
  }

  //it's required that argv[1] and argv[2] contain output file name and file open type
  FILE *output_file = fopen(argv[1], argv[2]);

  if (output_file == NULL)
  {
    printf("Warning: output file did not exist, file raport2.txt used.\n");
    system("touch raport2.txt");
    output_file = fopen("raport2.txt", "w");
  }

  block_array *array = NULL;
  int i=3;

  struct timespec real_start_time, real_end_time;
  struct tms start_times, end_times;

  while (i < argc)
  {
    times (&start_times);
    clock_gettime (CLOCK_REALTIME, &real_start_time);

    if (strcmp (argv[i], "create_table") == 0)
    {
      int array_length = 0;

      if (i+1 < argc)
      {
        if (is_a_number(argv[i+1]) != 0)
          array_length = atoi (argv[i+1]);
        else
        {
          printf("Error: provided size for table (argument %d) was not correct! Couldn't create array!",i);
          delete_block_array (array);
          exit(1);
        }
      }
      else
      {
        printf("Error: didn't provide argument for table size, argument list length was too short!");
        delete_block_array (array);
        exit(1);
      }

      delete_block_array(array);
      array = create_block_array (array_length);

      times (&end_times);
      clock_gettime (CLOCK_REALTIME, &real_end_time);

      char command[128];
      sprintf (command, "create_table %s \n", argv[i+1]);
      write_times (command, &real_start_time, &real_end_time, &start_times, &end_times, output_file);

      i += 2;
    }

    else if (strcmp (argv[i], "load_file") == 0)
    {
      if (i+1 >= argc)
      {
        printf("Error: didn't provide argument for file to be loaded, argument list length was too short!");
        delete_block_array (array);
        exit(1);
      }

      
      int loaded_file_index = load_tmp_file (array,argv[i+1]);
      loaded_file_index = loaded_file_index;

      times (&end_times);
      clock_gettime (CLOCK_REALTIME, &real_end_time);

      char command[128];
      sprintf (command, "load_file %s \n", argv[i+1]);
      write_times (command, &real_start_time, &real_end_time, &start_times, &end_times, output_file);

      i += 2;
    }

    else if (strcmp (argv[i], "remove_block") == 0)
    {
      if (i+1 < argc)
      {
        if (is_a_number(argv[i+1]) == 0)
        {
          printf("Error: provided index for block to be removed (argument %d) was not correct! Couldn't remove block!",i);
          delete_block_array (array);
          exit(1);
        }
      }
      else
      {
        printf("Error: didn't provide argument for block to be removed, argument list length was too short!");
        delete_block_array (array);
        exit(1);
      }

      delete_block (array, atoi (argv[i+1]));

      times (&end_times);
      clock_gettime (CLOCK_REALTIME, &real_end_time);

      char command[128];
      sprintf (command, "remove_block %s \n", argv[i+1]);
      write_times (command, &real_start_time, &real_end_time, &start_times, &end_times, output_file);

      i += 2;
    }

    else if (strcmp (argv[i], "remove_table") == 0)
    {
      delete_block_array (array);

      times (&end_times);
      clock_gettime (CLOCK_REALTIME, &real_end_time);

      char command[128];
      sprintf (command, "remove_table \n");
      write_times (command, &real_start_time, &real_end_time, &start_times, &end_times, output_file);

      i += 1;
    }

    else if (strcmp (argv[i], "search_directory") == 0)
    {
      if (i+3 >= argc)
      {
        printf("Error: didn't provide arguments for searching, argument list length was too short");
        delete_block_array (array);
        exit(1);
      }

      set_curr_search_pair (argv[i+1], argv[i+2]);

      search (argv[i+3]);

      times (&end_times);
      clock_gettime (CLOCK_REALTIME, &real_end_time);

      char command[128];
      sprintf (command, "search_directory %s %s %s \n", argv[i+1], argv[i+2], argv[i+3]);
      write_times (command, &real_start_time, &real_end_time, &start_times, &end_times, output_file);

      i += 4;
    }

    else if (strcmp (argv[i], "search_directory_curr_directory_and_file") == 0)
    {
      if (i+1 >= argc)
      {
        printf("Error: didn't provide argument for temporary file, argument list length was too short!");
        delete_block_array (array);
        exit(1);
      }

      search (argv[i+1]);

      times (&end_times);
      clock_gettime (CLOCK_REALTIME, &real_end_time);

      char command[128];
      sprintf (command, "search_directory_curr_directory_and_file %s \n", argv[i+1]);
      write_times (command, &real_start_time, &real_end_time, &start_times, &end_times, output_file);

      i += 2;
    }

    else if (strcmp (argv[i], "set_directory") == 0)
    {
      if (i+1 >= argc)
      {
        printf("Error: didn't provide argument for directory to be searched, argument list length was too short!");
        delete_block_array (array);
        exit(1);
      }

      set_curr_directory (argv[i+1]);

      times (&end_times);
      clock_gettime (CLOCK_REALTIME, &real_end_time);

      char command[128];
      sprintf (command, "set_directory %s \n", argv[i+1]);
      write_times (command, &real_start_time, &real_end_time, &start_times, &end_times, output_file);

      i += 2;
    }

    else if (strcmp (argv[i], "set_directory_and_file") == 0)
    {
      if (i+2 >= argc)
      {
        printf("Error: didn't provide arguments for directory and file for searching, argument list length was too short!");
        delete_block_array (array);
        exit(1);
      }

      set_curr_search_pair (argv[i+1], argv[i+2]);

      times (&end_times);
      clock_gettime (CLOCK_REALTIME, &real_end_time);

      char command[128];
      sprintf (command, "set_directory_and_file %s %s \n", argv[i+1], argv[i+2]);
      write_times (command, &real_start_time, &real_end_time, &start_times, &end_times, output_file);

      i += 3;
    }

    else if (strcmp (argv[i], "set_file") == 0)
    {
      if (i+1 >= argc)
      {
        printf("Error: didn't provide argument for file to be searched for, argument list length was too short!");
        delete_block_array (array);
        exit(1);
      }

      set_curr_file (argv[i+1]);

      times (&end_times);
      clock_gettime (CLOCK_REALTIME, &real_end_time);

      char command[128];
      sprintf (command, "set_file %s \n", argv[i+1]);
      write_times (command, &real_start_time, &real_end_time, &start_times, &end_times, output_file);

      i += 2;
    }

    else
    {
      printf("Unrecognized argument number %d!", i);
      delete_block_array (array);
      exit(1);
    }
  }

  delete_block_array (array);

  if (output_file != NULL)
  { fclose(output_file); }

  return 0;
}
