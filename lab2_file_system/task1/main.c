#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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

  sprintf(times,strcat(thing_measured, "Real time: %Ld ns, user time: %Ld ns, system time: %Ld ns\n"), real_time, user_time, system_time);

  fprintf(output_file, "%s \n", times);
}

// buffer size = record length (+1 for new line sign)
void copy (const char *file_name_from, const char *file_name_to, size_t number_of_records, size_t record_length, char *mode)
{
  char *buffer = calloc (record_length+1, sizeof(char));

  if (strcmp (mode, "sys") == 0)
  {
    // file descriptors, not actual files
    int file_from = open (file_name_from, O_RDONLY);
    int file_to = open (file_name_to, O_WRONLY|O_CREAT,0666);

    if (file_from == -1 || file_to == -1)
    {
      fprintf (stderr, "Error: could not open file for copying!");
      exit(1);
    }

    for (int i=0; i<number_of_records; i++)
    {
      read (file_from,buffer,record_length+1);
      write (file_to,buffer,record_length+1);
    }

    close (file_from);
    close (file_to);
  }
  else if (strcmp (mode, "lib") == 0)
  {
    // actual files
    FILE *file_from = fopen (file_name_from, "r+");
    FILE *file_to = fopen (file_name_to, "w");

    if (file_from == NULL || file_to == NULL)
    {
      fprintf (stderr, "Error: could not open file for copying!");
      exit(1);
    }

    for (int i=0; i<number_of_records; i++)
    {
      fread (buffer,sizeof(char),record_length+1,file_from);
      fwrite (buffer,sizeof(char),record_length+1,file_to);
    }

    fclose (file_from);
    fclose (file_to);
  }
  else
  {
    fprintf (stderr, "Error: copying mode was neither \"sys\" nor \"lib\"!");
    exit(1);
  }

  free (buffer);
}

void generate (const char *file_name, size_t number_of_records, size_t record_length)
{
  int file = open (file_name,O_WRONLY|O_CREAT,0666);

  for (int i=0; i<number_of_records; i++)
  {
    // record_length+1 because of new line character
    char *line = calloc (record_length+1, sizeof(char));
    for (int j=0; j<record_length; j++)
    { line[j] = (char) (rand()%26 + 97); }
    line[record_length] = '\n';

    write (file,line,record_length+1);
  }

  close(file);
}

void sort (const char *file_name, size_t number_of_records, size_t record_length, char *mode)
{
  record_length++; // for end of line signs
  char *current = calloc (record_length+1, sizeof(char));
  char *min = calloc (record_length+1, sizeof(char));

  if (current == NULL || min == NULL)
  {
    fprintf (stderr, "Error: could not allocate memory for record buffers for sorting!");
    exit(1);
  }

  if (strcmp (mode, "sys") == 0)
  {
    // file descriptors, not actual files
    int file = open (file_name, O_RDWR);

    if (file == -1)
    {
      fprintf (stderr, "Error: could not open file for sorting!");
      exit(1);
    }

    read (file, min, record_length);
    int minimum_number = -1;

    for (int i=0; i<number_of_records-1; i++)
    {
      int current_number = i+1;

      while ( read(file,current,record_length) == record_length)
      {
        if (current[0] < min[0])
        {
          strcpy (min,current);
          minimum_number = current_number;
        }
        current_number++;
      }

      if (minimum_number != -1)
      {
        lseek (file,i*record_length,SEEK_SET);
        read (file,current,record_length);

        lseek (file,minimum_number*record_length,SEEK_SET);
        write (file,current,record_length);
      }

      minimum_number = -1;

      lseek (file,i*record_length,SEEK_SET);
      write (file,min,record_length);
      read (file,min,record_length);
    }

    close (file);
  }
  else if (strcmp (mode, "lib") == 0)
  {
    // actual files
    FILE *file = fopen (file_name, "r+");

    if (file == NULL)
    {
      fprintf (stderr, "Error: could not open file for sorting!");
      exit(1);
    }

    fread (min,sizeof(char),record_length,file);
    int minimum_number = -1;

    for (int i=0; i<number_of_records-1; i++)
    {
      int current_number = i+1;

      while (fread(current,sizeof(char),record_length,file) == record_length)
      {
        if (current[0] < min[0])
        {
          strcpy (min,current);
          minimum_number = current_number;
        }
        current_number++;
      }

      if (minimum_number != -1)
      {
        fseek (file,i*record_length,0);
        fread (current,sizeof(char),record_length,file);

        fseek (file,minimum_number*record_length,0);
        fwrite (current,sizeof(char),record_length,file);
      }

      minimum_number = -1;

      fseek (file,i*record_length,0);
      fwrite (min,sizeof(char),record_length,file);
      fread (min,sizeof(char),record_length,file);
    }

    fclose(file);
  }
  else
  {
    fprintf (stderr, "Error: sorting mode was neither \"sys\" nor \"lib\"!");
    exit(1);
  }

  free (current);
  free(min);
}

int main (int argc, char **argv)
{
  struct tms start_times, end_times;
  struct timespec start_real, end_real;

  FILE *output_file = fopen("results.txt","a");

  if (output_file == NULL)
  {
    system("touch results.txt");
    output_file = fopen("results.txt", "a");
  }

  if (argc < 5)
  {
    fprintf (stderr, "Error: not enough arguments for any function!");
    exit(1);
  }

  if (strcmp(argv[1],"copy") == 0)
  {
    // args: programName, "copy", file_from, file_to, number_of_records, record_length, mode
    if (argc < 7)
    {
      fprintf (stderr, "Error: not enough arguments for copy! There should be: copy, file_from, file_to, number_of_records, record_length, mode");
      exit(1);
    }

    size_t number_of_records;

    if (is_a_number (argv[4]) != 0)
    {
      number_of_records = atoi(argv[4]);
    }
    else
    {
      fprintf (stderr, "Error: 5th argument number_of_records should be numeric!");
      exit(1);
    }

    size_t record_length;

    if (is_a_number (argv[5]) != 0)
    {
      record_length = atoi(argv[5]);
    }
    else
    {
      fprintf (stderr, "Error: 6th argument record_length should be numeric!");
      exit(1);
    }

    times(&start_times);
    clock_gettime(CLOCK_REALTIME,&start_real);

    copy (argv[2],argv[3],number_of_records,record_length,argv[6]);

    times(&end_times);
    clock_gettime(CLOCK_REALTIME,&end_real);

    char command[128];
    sprintf (command, "copy %s records from %s to %s \n", argv[4], argv[2], argv[3]);
    write_times (command, &start_real, &end_real, &start_times, &end_times, output_file);
  }
  else if (strcmp(argv[1],"generate") == 0)
  {
    // args: programName, "generate", file_name, number_of_records, record_length
    if (argc < 5)
    {
      fprintf (stderr, "Error: not enough arguments for copy! There should be: generate, file_name, number_of_records, record_length");
      exit(1);
    }

    size_t number_of_records;

    if (is_a_number (argv[3]) != 0)
    {
      number_of_records = atoi(argv[3]);
    }
    else
    {
      fprintf (stderr, "Error: 4th argument number_of_records should be numeric!");
      exit(1);
    }

    size_t record_length;

    if (is_a_number (argv[4]) != 0)
    {
      record_length = atoi(argv[4]);
    }
    else
    {
      fprintf (stderr, "Error: 5th argument record_length should be numeric!");
      exit(1);
    }

    times(&start_times);
    clock_gettime(CLOCK_REALTIME,&start_real);

    generate (argv[2],number_of_records,record_length);

    times(&end_times);
    clock_gettime(CLOCK_REALTIME,&end_real);

    char command[128];
    sprintf (command, "generate %s records to %s \n", argv[3], argv[2]);
    write_times (command, &start_real, &end_real, &start_times, &end_times, output_file);
  }
  else if (strcmp(argv[1],"sort") == 0)
  {
    // args: programName, "sort", file_name, number_of_records, record_length, mode
    if (argc < 6)
    {
      fprintf (stderr, "Error: not enough arguments for copy! There should be: sort, file_name, number_of_records, record_length, mode");
      exit(1);
    }

    size_t number_of_records;

    if (is_a_number (argv[3]) != 0)
    {
      number_of_records = atoi(argv[3]);
    }
    else
    {
      fprintf (stderr, "Error: 4th argument number_of_records should be numeric!");
      exit(1);
    }

    size_t record_length;

    if (is_a_number (argv[4]) != 0)
    {
      record_length = atoi(argv[4]);
    }
    else
    {
      fprintf (stderr, "Error: 5th argument record_length should be numeric!");
      exit(1);
    }

    times(&start_times);
    clock_gettime(CLOCK_REALTIME,&start_real);

    sort (argv[2],number_of_records,record_length,argv[5]);

    times(&end_times);
    clock_gettime(CLOCK_REALTIME,&end_real);

    char command[128];
    sprintf (command, "sort %s records in file %s \n", argv[3], argv[2]);
    write_times (command, &start_real, &end_real, &start_times, &end_times, output_file);
  }
  else
  {
    fprintf (stderr, "Error: wrong function was provided! It should be copy, generate or sort!");
    exit(1);
  }

  if (output_file != NULL)
  { fclose(output_file); }

  return 0;
}
