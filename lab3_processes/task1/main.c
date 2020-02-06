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
#include <wait.h>

void search_directory (char *directory_path)
{
  DIR *directory;
  struct dirent *element = NULL;

  directory = opendir (directory_path);

  if (directory != NULL)
  {
    while ( (element = readdir (directory)) != NULL)
    {
      char current_path[512];
      sprintf (current_path,"%s/%s",directory_path,element->d_name);

      struct stat file_info;
      lstat (current_path,&file_info);

      if ((file_info.st_mode & S_IFMT) == S_IFDIR)
      {
        if (strcmp(element->d_name,".") == 0 || strcmp(element->d_name,"..") == 0)
          continue;
        else
        {
          pid_t child_PID = fork();
          if (child_PID > 0)
          {
            waitpid (child_PID, NULL, 0);
          }
          else if (child_PID == 0)
          {
            closedir (directory);
            printf ("Current path: %s\nPID: %i\n", current_path, getpid());
            execl ("/bin/ls", "ls", current_path, "-l", NULL);
            exit(0);
          }
          else
          {
            perror("Error while creating child process!");
            exit(1);
          }
        }

        search_directory (current_path);
      }
    }
    closedir (directory);
  }
  else
  {
    perror ("Error: could not open directory!");
    exit(1);
  }
}

int main (int argc, char **argv)
{
  if (argc < 2)
  {
    perror("Error: directory argument required!");
    exit(1);
  }

  char *directory_path = argv[1];

  DIR *base_directory = opendir (directory_path);

  if (base_directory == NULL)
  {
    fputs("Error: unable to open provided directory!", stderr);
    exit(1);
  }

  search_directory (directory_path);

  return 0;
}
