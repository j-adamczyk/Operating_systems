#define _XOPEN_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

void search_directory (char *directory_path, char *relation, time_t file_mod_time)
{
  DIR *directory;
  struct dirent *element = NULL;

  directory = opendir (directory_path);

  if (directory != NULL)
  {
    while ( (element = readdir (directory)) != NULL)
    {
      char new_path[512];
      sprintf(new_path,"%s/%s",directory_path,element->d_name);

      struct stat file_info;
      lstat (new_path,&file_info);

      char actualpath [PATH_MAX+1];
      char *absolute_path = realpath(new_path, actualpath);

      char *file_type;

      switch (file_info.st_mode & S_IFMT)
      {
        case S_IFREG: file_type = "file"; break;
        case S_IFCHR: file_type = "char dev"; break;
        case S_IFBLK: file_type = "block dev"; break;
        case S_IFIFO: file_type = "fifo"; break;
        case S_IFLNK: file_type = "slink"; break;
        case S_IFSOCK: file_type = "socket"; break;
        case S_IFDIR: file_type = "dir";
          if (strcmp(element->d_name,".") == 0 || strcmp(element->d_name,"..") == 0)
            continue;
          else
          { search_directory (new_path, relation, file_mod_time); break; }
        default: file_type = "UNKNOWN"; break;
      }

      char file_size[32];
      sprintf (file_size,"%li",file_info.st_size);

      char last_access_time[20];
      struct tm *last_access_time_info = localtime (&file_info.st_atime);
      strftime (last_access_time,20,"%d/%m/%y %H:%M:%S",last_access_time_info);

      char last_modification_time[20];
      struct tm *last_modification_time_info = localtime (&file_info.st_mtime);
      strftime (last_modification_time,20,"%d/%m/%y %H:%M:%S",last_modification_time_info);

      if ( (strcmp (relation,">") && file_info.st_mtime < file_mod_time) || (strcmp (relation,"<") && file_info.st_mtime > file_mod_time) || file_info.st_mtime == file_mod_time )
        printf("Absolute file path: %s\nFile type: %s\nSize: %s bytes\nLast access time: %s\nLast modification time: %s\n\n",absolute_path,file_type,file_size,last_access_time,last_modification_time);
    }
    (void) closedir(directory);
  }
  else
  {
    char error_message[64];
    sprintf(error_message, "Error: could not open directory %s!", directory_path);
    fputs(error_message, stderr);
    exit(1);
  }
}

int main (int argc, char **argv)
{
  if (argc < 5)
  {
    fputs("Error: not enough arguments! They should be: directory </>/= DD/MM/YYYY HH:MM:SS", stderr);
    exit(1);
  }

  //first argument, directory
  char *directory_path = argv[1];

  // second argument, relation
  if (strcmp (argv[2], "<") != 0 && strcmp (argv[2], ">") != 0 && strcmp (argv[2], "=") != 0)
  {
    fputs("Error: second argument must be <, > or =!", stderr);
    exit(1);
  }

  char *relation = argv[2];

  // third and fourth arguments, date and time
  char date_input[20];
  struct tm date_and_time;

  sprintf(date_input, "%s %s", argv[3], argv[4]);

  if (strptime (date_input, "%d/%m/%y %H:%M:%S", &date_and_time) == NULL)
  {
    fputs("Error while parsing date and time!", stderr);
    exit(1);
  }

  time_t file_mod_time = mktime (&date_and_time);

  //real program
  DIR *base_directory = opendir (directory_path);

  if (base_directory == NULL)
  {
    fputs("Error: unable to open provided directory!", stderr);
    exit(1);
  }

  search_directory (directory_path, relation, file_mod_time);

  return 0;
}
