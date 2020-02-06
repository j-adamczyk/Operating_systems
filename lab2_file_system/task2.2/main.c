#define _XOPEN_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <ftw.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static char *relation;
static time_t file_mod_time;

int get_file_info (const char *file_path, const struct stat *file_info, int type, struct FTW *FTW_buffer)
{
  if (type == FTW_NS)
  {
    fprintf(stderr, "Error: couldn't read file %s!", file_path);
    exit(1);
  }

  char actualpath [PATH_MAX+1];
  char *absolute_path = realpath(file_path, actualpath);

  if ( (strcmp (relation,">") && file_info->st_mtime < file_mod_time) || (strcmp (relation,"<") && file_info->st_mtime > file_mod_time) || file_info->st_mtime == file_mod_time )
  {
    char *file_type;

    switch (file_info->st_mode & S_IFMT)
    {
      case S_IFREG: file_type = "file"; break;
      case S_IFCHR: file_type = "char dev"; break;
      case S_IFBLK: file_type = "block dev"; break;
      case S_IFIFO: file_type = "fifo"; break;
      case S_IFLNK: file_type = "slink"; break;
      case S_IFSOCK: file_type = "socket"; break;
      case S_IFDIR: file_type = "dir"; break;
      default: file_type = "UNKNOWN"; break;
    }

    char file_size[32];
    sprintf (file_size,"%li",file_info->st_size);

    char last_access_time[20];
    struct tm *last_access_time_info = localtime (&file_info->st_atime);
    strftime (last_access_time,20,"%d/%m/%y %H:%M:%S",last_access_time_info);

    char last_modification_time[20];
    struct tm *last_modification_time_info = localtime (&file_info->st_mtime);
    strftime (last_modification_time,20,"%d/%m/%y %H:%M:%S",last_modification_time_info);

    printf("Absolute file path: %s\nFile type: %s\nSize: %s bytes\nLast access date: %s\nLast status change date: %s\n\n",absolute_path,file_type,file_size,last_access_time,last_modification_time);
  }

  return 0; // continue flag for nftw
}

int main (int argc, char **argv)
{
  if (argc < 5)
  {
    fprintf(stderr, "Error: not enough arguments! They should be: directory </>/= DD/MM/YYYY HH:MM:SS");
    exit(1);
  }

  // first argument, directory
  char *directory_path = argv[1];


  // second argument, relation
  if (strcmp (argv[2], "<") != 0 && strcmp (argv[2], ">") != 0 && strcmp (argv[2], "=") != 0)
  {
    fprintf(stderr, "Error: second argument must be <, > or =!");
    exit(1);
  }

  relation = argv[2];


  // third and fourth arguments, date and time
  char date_input[20];
  struct tm date_and_time;

  sprintf(date_input, "%s %s", argv[3], argv[4]);

  if (strptime (date_input, "%d/%m/%y %H:%M:%S", &date_and_time) == NULL)
  {
    fprintf(stderr, "Error while parsing date and time!");
    exit(1);
  }

  file_mod_time = mktime (&date_and_time);


  // real program

  int flags = 0;
  flags |= FTW_PHYS; // do not follow symlinks

  if (nftw (directory_path, get_file_info, 20, flags) == -1)
  {
    perror("nftw");
    exit(1);
  }

  return 0;
}
